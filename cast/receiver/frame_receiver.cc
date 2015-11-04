// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cast/receiver/frame_receiver.h"

#include <algorithm>

#include "base/big_endian.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop/message_loop.h"
#include "media/cast/cast_environment.h"

namespace {
const int kMinSchedulingDelayMs = 1;
}  // namespace

namespace media {
namespace cast {

FrameReceiver::FrameReceiver(
    const scoped_refptr<CastEnvironment>& cast_environment,
    const FrameReceiverConfig& config,
    EventMediaType event_media_type,
    CastTransportSender* const transport)
    : cast_environment_(cast_environment),
      transport_(transport),
      packet_parser_(config.sender_ssrc, config.rtp_payload_type),
      stats_(cast_environment->Clock()),
      event_media_type_(event_media_type),
      event_subscriber_(kReceiverRtcpEventHistorySize, event_media_type),
      rtp_timebase_(config.rtp_timebase),
      target_playout_delay_(
          base::TimeDelta::FromMilliseconds(config.rtp_max_delay_ms)),
      expected_frame_duration_(
          base::TimeDelta::FromSeconds(1) / config.target_frame_rate),
      reports_are_scheduled_(false),
      framer_(cast_environment->Clock(),
              this,
              config.sender_ssrc,
              true,
              config.rtp_max_delay_ms * config.target_frame_rate / 1000),
      rtcp_(RtcpCastMessageCallback(),
            RtcpRttCallback(),
            RtcpLogMessageCallback(),
            cast_environment_->Clock(),
            NULL,
            config.receiver_ssrc,
            config.sender_ssrc),
      is_waiting_for_consecutive_frame_(false),
      lip_sync_drift_(ClockDriftSmoother::GetDefaultTimeConstant()),
      weak_factory_(this) {
  transport_->AddValidSsrc(config.sender_ssrc);
  DCHECK_GT(config.rtp_max_delay_ms, 0);
  DCHECK_GT(config.target_frame_rate, 0);
  decryptor_.Initialize(config.aes_key, config.aes_iv_mask);
  cast_environment_->logger()->Subscribe(&event_subscriber_);
  memset(frame_id_to_rtp_timestamp_, 0, sizeof(frame_id_to_rtp_timestamp_));
}

FrameReceiver::~FrameReceiver() {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  cast_environment_->logger()->Unsubscribe(&event_subscriber_);
}

void FrameReceiver::RequestEncodedFrame(
    const ReceiveEncodedFrameCallback& callback) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  frame_request_queue_.push_back(callback);
  EmitAvailableEncodedFrames();
}

bool FrameReceiver::ProcessPacket(scoped_ptr<Packet> packet) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));

  if (Rtcp::IsRtcpPacket(&packet->front(), packet->size())) {
    rtcp_.IncomingRtcpPacket(&packet->front(), packet->size());
  } else {
    RtpCastHeader rtp_header;
    const uint8* payload_data;
    size_t payload_size;
    if (!packet_parser_.ParsePacket(&packet->front(),
                                    packet->size(),
                                    &rtp_header,
                                    &payload_data,
                                    &payload_size)) {
      return false;
    }

    ProcessParsedPacket(rtp_header, payload_data, payload_size);
    stats_.UpdateStatistics(rtp_header);
  }

  if (!reports_are_scheduled_) {
    ScheduleNextRtcpReport();
    ScheduleNextCastMessage();
    reports_are_scheduled_ = true;
  }

  return true;
}

void FrameReceiver::ProcessParsedPacket(const RtpCastHeader& rtp_header,
                                        const uint8* payload_data,
                                        size_t payload_size) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));

  const base::TimeTicks now = cast_environment_->Clock()->NowTicks();

  frame_id_to_rtp_timestamp_[rtp_header.frame_id & 0xff] =
      rtp_header.rtp_timestamp;

  scoped_ptr<PacketEvent> receive_event(new PacketEvent());
  receive_event->timestamp = now;
  receive_event->type = PACKET_RECEIVED;
  receive_event->media_type = event_media_type_;
  receive_event->rtp_timestamp = rtp_header.rtp_timestamp;
  receive_event->frame_id = rtp_header.frame_id;
  receive_event->packet_id = rtp_header.packet_id;
  receive_event->max_packet_id = rtp_header.max_packet_id;
  receive_event->size = payload_size;
  cast_environment_->logger()->DispatchPacketEvent(receive_event.Pass());

  bool duplicate = false;
  const bool complete =
      framer_.InsertPacket(payload_data, payload_size, rtp_header, &duplicate);

  // Duplicate packets are ignored.
  if (duplicate)
    return;

  // Update lip-sync values upon receiving the first packet of each frame, or if
  // they have never been set yet.
  if (rtp_header.packet_id == 0 || lip_sync_reference_time_.is_null()) {
    RtpTimestamp fresh_sync_rtp;
    base::TimeTicks fresh_sync_reference;
    if (!rtcp_.GetLatestLipSyncTimes(&fresh_sync_rtp, &fresh_sync_reference)) {
      // HACK: The sender should have provided Sender Reports before the first
      // frame was sent.  However, the spec does not currently require this.
      // Therefore, when the data is missing, the local clock is used to
      // generate reference timestamps.
      VLOG(2) << "Lip sync info missing.  Falling-back to local clock.";
      fresh_sync_rtp = rtp_header.rtp_timestamp;
      fresh_sync_reference = now;
    }
    // |lip_sync_reference_time_| is always incremented according to the time
    // delta computed from the difference in RTP timestamps.  Then,
    // |lip_sync_drift_| accounts for clock drift and also smoothes-out any
    // sudden/discontinuous shifts in the series of reference time values.
    if (lip_sync_reference_time_.is_null()) {
      lip_sync_reference_time_ = fresh_sync_reference;
    } else {
      lip_sync_reference_time_ += RtpDeltaToTimeDelta(
          static_cast<int32>(fresh_sync_rtp - lip_sync_rtp_timestamp_),
          rtp_timebase_);
    }
    lip_sync_rtp_timestamp_ = fresh_sync_rtp;
    lip_sync_drift_.Update(
        now, fresh_sync_reference - lip_sync_reference_time_);
  }

  // Another frame is complete from a non-duplicate packet.  Attempt to emit
  // more frames to satisfy enqueued requests.
  if (complete)
    EmitAvailableEncodedFrames();
}

void FrameReceiver::CastFeedback(const RtcpCastMessage& cast_message) {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));

  base::TimeTicks now = cast_environment_->Clock()->NowTicks();
  RtpTimestamp rtp_timestamp =
      frame_id_to_rtp_timestamp_[cast_message.ack_frame_id & 0xff];

  scoped_ptr<FrameEvent> ack_sent_event(new FrameEvent());
  ack_sent_event->timestamp = now;
  ack_sent_event->type = FRAME_ACK_SENT;
  ack_sent_event->media_type = event_media_type_;
  ack_sent_event->rtp_timestamp = rtp_timestamp;
  ack_sent_event->frame_id = cast_message.ack_frame_id;
  cast_environment_->logger()->DispatchFrameEvent(ack_sent_event.Pass());

  ReceiverRtcpEventSubscriber::RtcpEvents rtcp_events;
  event_subscriber_.GetRtcpEventsWithRedundancy(&rtcp_events);
  transport_->SendRtcpFromRtpReceiver(rtcp_.GetLocalSsrc(),
                                      rtcp_.GetRemoteSsrc(),
                                      rtcp_.ConvertToNTPAndSave(now),
                                      &cast_message,
                                      target_playout_delay_,
                                      &rtcp_events,
                                      NULL);
}

void FrameReceiver::EmitAvailableEncodedFrames() {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));

  while (!frame_request_queue_.empty()) {
    // Attempt to peek at the next completed frame from the |framer_|.
    // TODO(miu): We should only be peeking at the metadata, and not copying the
    // payload yet!  Or, at least, peek using a StringPiece instead of a copy.
    scoped_ptr<EncodedFrame> encoded_frame(
        new EncodedFrame());
    bool is_consecutively_next_frame = false;
    bool have_multiple_complete_frames = false;
    if (!framer_.GetEncodedFrame(encoded_frame.get(),
                                 &is_consecutively_next_frame,
                                 &have_multiple_complete_frames)) {
      VLOG(1) << "Wait for more packets to produce a completed frame.";
      return;  // ProcessParsedPacket() will invoke this method in the future.
    }

    const base::TimeTicks now = cast_environment_->Clock()->NowTicks();
    const base::TimeTicks playout_time = GetPlayoutTime(*encoded_frame);

    // If we have multiple decodable frames, and the current frame is
    // too old, then skip it and decode the next frame instead.
    if (have_multiple_complete_frames && now > playout_time) {
      framer_.ReleaseFrame(encoded_frame->frame_id);
      continue;
    }

    // If |framer_| has a frame ready that is out of sequence, examine the
    // playout time to determine whether it's acceptable to continue, thereby
    // skipping one or more frames.  Skip if the missing frame wouldn't complete
    // playing before the start of playback of the available frame.
    if (!is_consecutively_next_frame) {
      // This assumes that decoding takes as long as playing, which might
      // not be true.
      const base::TimeTicks earliest_possible_end_time_of_missing_frame =
          now + expected_frame_duration_ * 2;
      if (earliest_possible_end_time_of_missing_frame < playout_time) {
        VLOG(1) << "Wait for next consecutive frame instead of skipping.";
        if (!is_waiting_for_consecutive_frame_) {
          is_waiting_for_consecutive_frame_ = true;
          cast_environment_->PostDelayedTask(
              CastEnvironment::MAIN,
              FROM_HERE,
              base::Bind(&FrameReceiver::EmitAvailableEncodedFramesAfterWaiting,
                         weak_factory_.GetWeakPtr()),
              playout_time - now);
        }
        return;
      }
    }

    // At this point, we have the complete next frame, or a decodable
    // frame from somewhere later in the stream, AND we have given up
    // on waiting for any frames in between, so now we can ACK the frame.
    framer_.AckFrame(encoded_frame->frame_id);

    // Decrypt the payload data in the frame, if crypto is being used.
    if (decryptor_.is_activated()) {
      std::string decrypted_data;
      if (!decryptor_.Decrypt(encoded_frame->frame_id,
                              encoded_frame->data,
                              &decrypted_data)) {
        // Decryption failed.  Give up on this frame.
        framer_.ReleaseFrame(encoded_frame->frame_id);
        continue;
      }
      encoded_frame->data.swap(decrypted_data);
    }

    // At this point, we have a decrypted EncodedFrame ready to be emitted.
    encoded_frame->reference_time = playout_time;
    framer_.ReleaseFrame(encoded_frame->frame_id);
    if (encoded_frame->new_playout_delay_ms) {
      target_playout_delay_ = base::TimeDelta::FromMilliseconds(
          encoded_frame->new_playout_delay_ms);
    }
    cast_environment_->PostTask(CastEnvironment::MAIN,
                                FROM_HERE,
                                base::Bind(&FrameReceiver::EmitOneFrame,
                                           weak_factory_.GetWeakPtr(),
                                           frame_request_queue_.front(),
                                           base::Passed(&encoded_frame)));
    frame_request_queue_.pop_front();
  }
}

void FrameReceiver::EmitAvailableEncodedFramesAfterWaiting() {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  DCHECK(is_waiting_for_consecutive_frame_);
  is_waiting_for_consecutive_frame_ = false;
  EmitAvailableEncodedFrames();
}

void FrameReceiver::EmitOneFrame(const ReceiveEncodedFrameCallback& callback,
                                 scoped_ptr<EncodedFrame> encoded_frame) const {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  if (!callback.is_null())
    callback.Run(encoded_frame.Pass());
}

base::TimeTicks FrameReceiver::GetPlayoutTime(const EncodedFrame& frame) const {
  base::TimeDelta target_playout_delay = target_playout_delay_;
  if (frame.new_playout_delay_ms) {
    target_playout_delay = base::TimeDelta::FromMilliseconds(
        frame.new_playout_delay_ms);
  }
  return lip_sync_reference_time_ +
      lip_sync_drift_.Current() +
      RtpDeltaToTimeDelta(
          static_cast<int32>(frame.rtp_timestamp - lip_sync_rtp_timestamp_),
          rtp_timebase_) +
      target_playout_delay;
}

void FrameReceiver::ScheduleNextCastMessage() {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  base::TimeTicks send_time;
  framer_.TimeToSendNextCastMessage(&send_time);
  base::TimeDelta time_to_send =
      send_time - cast_environment_->Clock()->NowTicks();
  time_to_send = std::max(
      time_to_send, base::TimeDelta::FromMilliseconds(kMinSchedulingDelayMs));
  cast_environment_->PostDelayedTask(
      CastEnvironment::MAIN,
      FROM_HERE,
      base::Bind(&FrameReceiver::SendNextCastMessage,
                 weak_factory_.GetWeakPtr()),
      time_to_send);
}

void FrameReceiver::SendNextCastMessage() {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  framer_.SendCastMessage();  // Will only send a message if it is time.
  ScheduleNextCastMessage();
}

void FrameReceiver::ScheduleNextRtcpReport() {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));

  cast_environment_->PostDelayedTask(
      CastEnvironment::MAIN, FROM_HERE,
      base::Bind(&FrameReceiver::SendNextRtcpReport,
                 weak_factory_.GetWeakPtr()),
      base::TimeDelta::FromMilliseconds(kDefaultRtcpIntervalMs));
}

void FrameReceiver::SendNextRtcpReport() {
  DCHECK(cast_environment_->CurrentlyOn(CastEnvironment::MAIN));
  const base::TimeTicks now = cast_environment_->Clock()->NowTicks();
  RtpReceiverStatistics stats = stats_.GetStatistics();
  transport_->SendRtcpFromRtpReceiver(rtcp_.GetLocalSsrc(),
                                      rtcp_.GetRemoteSsrc(),
                                      rtcp_.ConvertToNTPAndSave(now),
                                      NULL,
                                      base::TimeDelta(),
                                      NULL,
                                      &stats);
  ScheduleNextRtcpReport();
}

}  // namespace cast
}  // namespace media
