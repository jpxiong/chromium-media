// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/android/media_codec_audio_decoder.h"

#include "base/android/build_info.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/threading/thread_task_runner_handle.h"
#include "media/base/android/sdk_media_codec_bridge.h"
#include "media/base/audio_buffer.h"
#include "media/base/audio_timestamp_helper.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/timestamp_constants.h"

namespace media {

MediaCodecAudioDecoder::MediaCodecAudioDecoder(
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : task_runner_(task_runner),
      state_(STATE_UNINITIALIZED),
      channel_count_(0),
      channel_layout_(CHANNEL_LAYOUT_NONE),
      sample_rate_(0),
      media_drm_bridge_cdm_context_(nullptr),
      cdm_registration_id_(0),
      weak_factory_(this) {
  DVLOG(1) << __func__;
}

MediaCodecAudioDecoder::~MediaCodecAudioDecoder() {
  DVLOG(1) << __func__;

  codec_loop_.reset();

  if (media_drm_bridge_cdm_context_) {
    DCHECK(cdm_registration_id_);

    // Cancel previously registered callback (if any).
    media_drm_bridge_cdm_context_->SetMediaCryptoReadyCB(
        MediaDrmBridgeCdmContext::MediaCryptoReadyCB());

    media_drm_bridge_cdm_context_->UnregisterPlayer(cdm_registration_id_);
  }

  ClearInputQueue(DecodeStatus::ABORTED);
}

std::string MediaCodecAudioDecoder::GetDisplayName() const {
  return "MediaCodecAudioDecoder";
}

void MediaCodecAudioDecoder::Initialize(const AudioDecoderConfig& config,
                                        CdmContext* cdm_context,
                                        const InitCB& init_cb,
                                        const OutputCB& output_cb) {
  DVLOG(1) << __func__ << ": " << config.AsHumanReadableString();
  DCHECK_EQ(state_, STATE_UNINITIALIZED);

  InitCB bound_init_cb = BindToCurrentLoop(init_cb);

  // We can support only the codecs that AudioCodecBridge can decode.
  const bool is_codec_supported = config.codec() == kCodecVorbis ||
                                  config.codec() == kCodecAAC ||
                                  config.codec() == kCodecOpus;
  if (!is_codec_supported) {
    DVLOG(1) << "Unsuported codec " << GetCodecName(config.codec());
    bound_init_cb.Run(false);
    return;
  }

  if (config.is_encrypted() && !cdm_context) {
    NOTREACHED() << "The stream is encrypted but there is no CDM context";
    bound_init_cb.Run(false);
    return;
  }

  config_ = config;
  output_cb_ = BindToCurrentLoop(output_cb);

  SetInitialConfiguration();

  if (config_.is_encrypted()) {
    // Postpone initialization after MediaCrypto is available.
    // SetCdm uses init_cb in a method that's already bound to the current loop.
    SetCdm(cdm_context, init_cb);
    SetState(STATE_WAITING_FOR_MEDIA_CRYPTO);
    return;
  }

  if (!CreateMediaCodecLoop()) {
    bound_init_cb.Run(false);
    return;
  }

  SetState(STATE_READY);
  bound_init_cb.Run(true);
}

bool MediaCodecAudioDecoder::CreateMediaCodecLoop() {
  DVLOG(1) << __func__ << ": config:" << config_.AsHumanReadableString();

  codec_loop_.reset();

  std::unique_ptr<AudioCodecBridge> audio_codec_bridge(
      AudioCodecBridge::Create(config_.codec()));
  if (!audio_codec_bridge) {
    DLOG(ERROR) << __func__ << " failed: cannot create AudioCodecBridge";
    return false;
  }

  jobject media_crypto_obj = media_crypto_ ? media_crypto_->obj() : nullptr;

  if (!audio_codec_bridge->ConfigureAndStart(config_, media_crypto_obj)) {
    DLOG(ERROR) << __func__ << " failed: cannot configure audio codec for "
                << config_.AsHumanReadableString();
    return false;
  }

  codec_loop_.reset(
      new MediaCodecLoop(base::android::BuildInfo::GetInstance()->sdk_int(),
                         this, std::move(audio_codec_bridge)));

  return true;
}

void MediaCodecAudioDecoder::Decode(const scoped_refptr<DecoderBuffer>& buffer,
                                    const DecodeCB& decode_cb) {
  DecodeCB bound_decode_cb = BindToCurrentLoop(decode_cb);

  if (!buffer->end_of_stream() && buffer->timestamp() == kNoTimestamp) {
    DVLOG(2) << __func__ << " " << buffer->AsHumanReadableString()
             << ": no timestamp, skipping this buffer";
    bound_decode_cb.Run(DecodeStatus::DECODE_ERROR);
    return;
  }

  // Note that we transition to STATE_ERROR if |codec_loop_| does.
  if (state_ == STATE_ERROR) {
    // We get here if an error happens in DequeueOutput() or Reset().
    DVLOG(2) << __func__ << " " << buffer->AsHumanReadableString()
             << ": Error state, returning decode error for all buffers";
    ClearInputQueue(DecodeStatus::DECODE_ERROR);
    bound_decode_cb.Run(DecodeStatus::DECODE_ERROR);
    return;
  }

  DCHECK(codec_loop_);

  DVLOG(2) << __func__ << " " << buffer->AsHumanReadableString();

  DCHECK_EQ(state_, STATE_READY) << " unexpected state " << AsString(state_);

  // AudioDecoder requires that "Only one decode may be in flight at any given
  // time".
  DCHECK(input_queue_.empty());

  input_queue_.push_back(std::make_pair(buffer, bound_decode_cb));

  codec_loop_->DoPendingWork();
}

void MediaCodecAudioDecoder::Reset(const base::Closure& closure) {
  DVLOG(1) << __func__;

  ClearInputQueue(DecodeStatus::ABORTED);

  // Flush if we can, otherwise completely recreate and reconfigure the codec.
  bool success = codec_loop_->TryFlush();

  // If the flush failed, then we have to re-create the codec.
  if (!success)
    success = CreateMediaCodecLoop();

  timestamp_helper_->SetBaseTimestamp(kNoTimestamp);

  SetState(success ? STATE_READY : STATE_ERROR);

  task_runner_->PostTask(FROM_HERE, closure);
}

bool MediaCodecAudioDecoder::NeedsBitstreamConversion() const {
  // An AAC stream needs to be converted as ADTS stream.
  DCHECK_NE(config_.codec(), kUnknownAudioCodec);
  return config_.codec() == kCodecAAC;
}

void MediaCodecAudioDecoder::SetCdm(CdmContext* cdm_context,
                                    const InitCB& init_cb) {
  DCHECK(cdm_context);

  // On Android platform the CdmContext must be a MediaDrmBridgeCdmContext.
  media_drm_bridge_cdm_context_ =
      static_cast<media::MediaDrmBridgeCdmContext*>(cdm_context);

  // Register CDM callbacks. The callbacks registered will be posted back to
  // this thread via BindToCurrentLoop.

  // Since |this| holds a reference to the |cdm_|, by the time the CDM is
  // destructed, UnregisterPlayer() must have been called and |this| has been
  // destructed as well. So the |cdm_unset_cb| will never have a chance to be
  // called.
  // TODO(xhwang): Remove |cdm_unset_cb| after it's not used on all platforms.
  cdm_registration_id_ = media_drm_bridge_cdm_context_->RegisterPlayer(
      media::BindToCurrentLoop(base::Bind(&MediaCodecAudioDecoder::OnKeyAdded,
                                          weak_factory_.GetWeakPtr())),
      base::Bind(&base::DoNothing));

  media_drm_bridge_cdm_context_->SetMediaCryptoReadyCB(media::BindToCurrentLoop(
      base::Bind(&MediaCodecAudioDecoder::OnMediaCryptoReady,
                 weak_factory_.GetWeakPtr(), init_cb)));
}

void MediaCodecAudioDecoder::OnKeyAdded() {
  DVLOG(1) << __func__;

  // We don't register |codec_loop_| directly with the DRM bridge, since it's
  // subject to replacement.
  if (codec_loop_)
    codec_loop_->OnKeyAdded();
}

void MediaCodecAudioDecoder::OnMediaCryptoReady(
    const InitCB& init_cb,
    MediaDrmBridgeCdmContext::JavaObjectPtr media_crypto,
    bool /*needs_protected_surface*/) {
  DVLOG(1) << __func__;

  DCHECK(state_ == STATE_WAITING_FOR_MEDIA_CRYPTO);

  if (!media_crypto) {
    LOG(ERROR) << "MediaCrypto is not available, can't play encrypted stream.";
    SetState(STATE_UNINITIALIZED);
    init_cb.Run(false);
    return;
  }

  DCHECK(!media_crypto->is_null());
  media_crypto_ = std::move(media_crypto);

  // We assume this is a part of the initialization process, thus MediaCodec
  // is not created yet.
  DCHECK(!codec_loop_);

  // After receiving |media_crypto_| we can configure MediaCodec.
  if (!CreateMediaCodecLoop()) {
    SetState(STATE_UNINITIALIZED);
    init_cb.Run(false);
    return;
  }

  SetState(STATE_READY);
  init_cb.Run(true);
}

bool MediaCodecAudioDecoder::IsAnyInputPending() const {
  if (state_ != STATE_READY)
    return false;

  return !input_queue_.empty();
}

MediaCodecLoop::InputData MediaCodecAudioDecoder::ProvideInputData() {
  DVLOG(2) << __func__;

  scoped_refptr<DecoderBuffer> decoder_buffer = input_queue_.front().first;

  MediaCodecLoop::InputData input_data;
  if (decoder_buffer->end_of_stream()) {
    input_data.is_eos = true;
  } else {
    input_data.memory = static_cast<const uint8_t*>(decoder_buffer->data());
    input_data.length = decoder_buffer->data_size();
    const DecryptConfig* decrypt_config = decoder_buffer->decrypt_config();
    if (decrypt_config && decrypt_config->is_encrypted()) {
      input_data.key_id = decrypt_config->key_id();
      input_data.iv = decrypt_config->iv();
      input_data.subsamples = decrypt_config->subsamples();
      input_data.encryption_scheme = config_.encryption_scheme();
    }
    input_data.presentation_time = decoder_buffer->timestamp();
  }

  // We do not pop |input_queue_| here.  MediaCodecLoop may refer to data that
  // it owns until OnInputDataQueued is called.

  return input_data;
}

void MediaCodecAudioDecoder::OnInputDataQueued(bool success) {
  // If this is an EOS buffer, then wait to call back until we are notified that
  // it has been processed via OnDecodedEos().  If the EOS was not queued
  // successfully, then we do want to signal error now since there is no queued
  // EOS to process later.
  if (input_queue_.front().first->end_of_stream() && success)
    return;

  input_queue_.front().second.Run(success ? DecodeStatus::OK
                                          : DecodeStatus::DECODE_ERROR);
  input_queue_.pop_front();
}

void MediaCodecAudioDecoder::ClearInputQueue(DecodeStatus decode_status) {
  DVLOG(2) << __func__;

  for (const auto& entry : input_queue_)
    entry.second.Run(decode_status);

  input_queue_.clear();
}

void MediaCodecAudioDecoder::SetState(State new_state) {
  DVLOG(1) << __func__ << ": " << AsString(state_) << "->"
           << AsString(new_state);
  state_ = new_state;
}

void MediaCodecAudioDecoder::OnCodecLoopError() {
  // If the codec transitions into the error state, then so should we.
  SetState(STATE_ERROR);
  ClearInputQueue(DecodeStatus::DECODE_ERROR);
}

void MediaCodecAudioDecoder::OnDecodedEos(
    const MediaCodecLoop::OutputBuffer& out) {
  DVLOG(2) << __func__ << " pts:" << out.pts;

  // If we've transitioned into the error state, then we don't really know what
  // to do.  If we transitioned because of OnCodecError, then all of our
  // buffers have been returned anyway.  Otherwise, it's unclear.  Note that
  // MCL does not call us back after OnCodecError(), since it stops decoding.
  // So, we shouldn't be in that state.  So, just DCHECK here.
  DCHECK_NE(state_, STATE_ERROR);
  DCHECK(input_queue_.size());
  DCHECK(input_queue_.front().first->end_of_stream());
  input_queue_.front().second.Run(DecodeStatus::OK);
  input_queue_.pop_front();
}

bool MediaCodecAudioDecoder::OnDecodedFrame(
    const MediaCodecLoop::OutputBuffer& out) {
  DVLOG(2) << __func__ << " pts:" << out.pts;

  DCHECK_NE(out.size, 0U);
  DCHECK_NE(out.index, MediaCodecLoop::kInvalidBufferIndex);
  DCHECK(codec_loop_);
  MediaCodecBridge* media_codec = codec_loop_->GetCodec();
  DCHECK(media_codec);

  // For proper |frame_count| calculation we need to use the actual number
  // of channels which can be different from |config_| value.
  DCHECK_GT(channel_count_, 0);

  // Android MediaCodec can only output 16bit PCM audio.
  const int bytes_per_frame = sizeof(uint16_t) * channel_count_;
  const size_t frame_count = out.size / bytes_per_frame;

  // Create AudioOutput buffer based on current parameters.
  scoped_refptr<AudioBuffer> audio_buffer =
      AudioBuffer::CreateBuffer(kSampleFormatS16, channel_layout_,
                                channel_count_, sample_rate_, frame_count);

  // Copy data into AudioBuffer.
  CHECK_LE(out.size, audio_buffer->data_size());

  MediaCodecStatus status = media_codec->CopyFromOutputBuffer(
      out.index, out.offset, audio_buffer->channel_data()[0], out.size);

  // Release MediaCodec output buffer.
  media_codec->ReleaseOutputBuffer(out.index, false);

  if (status != MEDIA_CODEC_OK)
    return false;

  // Calculate and set buffer timestamp.

  const bool first_buffer = timestamp_helper_->base_timestamp() == kNoTimestamp;
  if (first_buffer) {
    // Clamp the base timestamp to zero.
    timestamp_helper_->SetBaseTimestamp(std::max(base::TimeDelta(), out.pts));
  }

  audio_buffer->set_timestamp(timestamp_helper_->GetTimestamp());
  timestamp_helper_->AddFrames(frame_count);

  // Call the |output_cb_|.
  output_cb_.Run(audio_buffer);

  return true;
}

bool MediaCodecAudioDecoder::OnOutputFormatChanged() {
  DVLOG(2) << __func__;
  MediaCodecBridge* media_codec = codec_loop_->GetCodec();

  // Note that if we return false to transition |codec_loop_| to the error
  // state, then we'll also transition to the error state when it notifies us.

  int new_sampling_rate = 0;
  MediaCodecStatus status =
      media_codec->GetOutputSamplingRate(&new_sampling_rate);
  if (status != MEDIA_CODEC_OK) {
    DLOG(ERROR) << "GetOutputSamplingRate failed.";
    return false;
  }
  if (new_sampling_rate != sample_rate_) {
    DVLOG(1) << __func__ << ": detected sample rate change: " << sample_rate_
             << " -> " << new_sampling_rate;

    sample_rate_ = new_sampling_rate;
    const base::TimeDelta base_timestamp =
        timestamp_helper_->base_timestamp() == kNoTimestamp
            ? kNoTimestamp
            : timestamp_helper_->GetTimestamp();
    timestamp_helper_.reset(new AudioTimestampHelper(sample_rate_));
    if (base_timestamp != kNoTimestamp)
      timestamp_helper_->SetBaseTimestamp(base_timestamp);
  }

  int new_channel_count = 0;
  status = media_codec->GetOutputChannelCount(&new_channel_count);
  if (status != MEDIA_CODEC_OK || !new_channel_count) {
    DLOG(ERROR) << "GetOutputChannelCount failed.";
    return false;
  }

  if (new_channel_count != channel_count_) {
    DVLOG(1) << __func__
             << ": detected channel count change: " << channel_count_ << " -> "
             << new_channel_count;
    channel_count_ = new_channel_count;
    channel_layout_ = GuessChannelLayout(channel_count_);
  }

  return true;
}

void MediaCodecAudioDecoder::SetInitialConfiguration() {
  // Guess the channel count from |config_| in case OnOutputFormatChanged
  // that delivers the true count is not called before the first data arrives.
  // It seems upon certain input errors a codec may substitute silence and
  // not call OnOutputFormatChanged in this case.
  channel_layout_ = config_.channel_layout();
  channel_count_ = ChannelLayoutToChannelCount(channel_layout_);

  sample_rate_ = config_.samples_per_second();
  timestamp_helper_.reset(new AudioTimestampHelper(sample_rate_));
}

#undef RETURN_STRING
#define RETURN_STRING(x) \
  case x:                \
    return #x;

// static
const char* MediaCodecAudioDecoder::AsString(State state) {
  switch (state) {
    RETURN_STRING(STATE_UNINITIALIZED);
    RETURN_STRING(STATE_WAITING_FOR_MEDIA_CRYPTO);
    RETURN_STRING(STATE_READY);
    RETURN_STRING(STATE_ERROR);
  }
  NOTREACHED() << "Unknown state " << state;
  return nullptr;
}

#undef RETURN_STRING

}  // namespace media
