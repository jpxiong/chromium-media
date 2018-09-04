// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/windows/d3d11_video_decoder.h"

#include <d3d11_4.h>

#include "base/bind.h"
#include "base/callback.h"
#include "base/metrics/histogram_macros.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/decoder_buffer.h"
#include "media/base/media_log.h"
#include "media/base/video_codecs.h"
#include "media/base/video_decoder_config.h"
#include "media/base/video_frame.h"
#include "media/gpu/windows/d3d11_video_decoder_impl.h"

namespace {

// Check |weak_ptr| and run |cb| with |args| if it's non-null.
template <typename T, typename... Args>
void CallbackOnProperThread(base::WeakPtr<T> weak_ptr,
                            base::Callback<void(Args...)> cb,
                            Args... args) {
  if (weak_ptr.get())
    cb.Run(args...);
}

// Given a callback, |cb|, return another callback that will call |cb| after
// switching to the thread that BindToCurrent.... is called on.  We will check
// |weak_ptr| on the current thread.  This is different than just calling
// BindToCurrentLoop because we'll check the weak ptr.  If |cb| is some method
// of |T|, then one can use BindToCurrentLoop directly.  However, in our case,
// we have some unrelated callback that we'd like to call only if we haven't
// been destroyed yet.  I suppose this could also just be a method:
// template<CB, ...> D3D11VideoDecoder::CallSomeCallback(CB, ...) that's bound
// via BindToCurrentLoop directly.
template <typename T, typename... Args>
base::Callback<void(Args...)> BindToCurrentThreadIfWeakPtr(
    base::WeakPtr<T> weak_ptr,
    base::Callback<void(Args...)> cb) {
  return media::BindToCurrentLoop(
      base::Bind(&CallbackOnProperThread<T, Args...>, weak_ptr, cb));
}
}  // namespace

namespace media {

std::unique_ptr<VideoDecoder> D3D11VideoDecoder::Create(
    scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner,
    std::unique_ptr<MediaLog> media_log,
    const gpu::GpuPreferences& gpu_preferences,
    const gpu::GpuDriverBugWorkarounds& gpu_workarounds,
    base::RepeatingCallback<gpu::CommandBufferStub*()> get_stub_cb) {
  // We create |impl_| on the wrong thread, but we never use it here.
  // Note that the output callback will hop to our thread, post the video
  // frame, and along with a callback that will hop back to the impl thread
  // when it's released.
  // Note that we WrapUnique<VideoDecoder> rather than D3D11VideoDecoder to make
  // this castable; the deleters have to match.
  std::unique_ptr<MediaLog> cloned_media_log = media_log->Clone();
  return base::WrapUnique<VideoDecoder>(
      new D3D11VideoDecoder(std::move(gpu_task_runner), std::move(media_log),
                            gpu_preferences, gpu_workarounds,
                            std::make_unique<D3D11VideoDecoderImpl>(
                                std::move(cloned_media_log), get_stub_cb)));
}

D3D11VideoDecoder::D3D11VideoDecoder(
    scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner,
    std::unique_ptr<MediaLog> media_log,
    const gpu::GpuPreferences& gpu_preferences,
    const gpu::GpuDriverBugWorkarounds& gpu_workarounds,
    std::unique_ptr<D3D11VideoDecoderImpl> impl)
    : media_log_(std::move(media_log)),
      impl_(std::move(impl)),
      impl_task_runner_(std::move(gpu_task_runner)),
      gpu_preferences_(gpu_preferences),
      gpu_workarounds_(gpu_workarounds),
      create_device_func_(base::BindRepeating(D3D11CreateDevice)),
      weak_factory_(this) {
  impl_weak_ = impl_->GetWeakPtr();
}

D3D11VideoDecoder::~D3D11VideoDecoder() {
  // Post destruction to the main thread.  When this executes, it will also
  // cancel pending callbacks into |impl_| via |impl_weak_|.  Callbacks out
  // from |impl_| will be cancelled by |weak_factory_| when we return.
  if (impl_task_runner_->RunsTasksInCurrentSequence())
    impl_.reset();
  else
    impl_task_runner_->DeleteSoon(FROM_HERE, std::move(impl_));
}

std::string D3D11VideoDecoder::GetDisplayName() const {
  return "D3D11VideoDecoder";
}

void D3D11VideoDecoder::Initialize(
    const VideoDecoderConfig& config,
    bool low_delay,
    CdmContext* cdm_context,
    const InitCB& init_cb,
    const OutputCB& output_cb,
    const WaitingForDecryptionKeyCB& waiting_for_decryption_key_cb) {
  if (!IsPotentiallySupported(config)) {
    DVLOG(3) << "D3D11 video decoder not supported for the config.";
    init_cb.Run(false);
    return;
  }

  if (impl_task_runner_->RunsTasksInCurrentSequence()) {
    impl_->Initialize(config, low_delay, cdm_context, init_cb, output_cb,
                      waiting_for_decryption_key_cb);
    return;
  }

  // Bind our own init / output cb that hop to this thread, so we don't call
  // the originals on some other thread.
  // Important but subtle note: base::Bind will copy |config_| since it's a
  // const ref.
  // TODO(liberato): what's the lifetime of |cdm_context|?
  impl_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &VideoDecoder::Initialize, impl_weak_, config, low_delay, cdm_context,
          BindToCurrentThreadIfWeakPtr(weak_factory_.GetWeakPtr(), init_cb),
          BindToCurrentThreadIfWeakPtr(weak_factory_.GetWeakPtr(), output_cb),
          BindToCurrentThreadIfWeakPtr(weak_factory_.GetWeakPtr(),
                                       waiting_for_decryption_key_cb)));
}

void D3D11VideoDecoder::Decode(scoped_refptr<DecoderBuffer> buffer,
                               const DecodeCB& decode_cb) {
  if (impl_task_runner_->RunsTasksInCurrentSequence()) {
    impl_->Decode(std::move(buffer), decode_cb);
    return;
  }

  impl_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &VideoDecoder::Decode, impl_weak_, std::move(buffer),
          BindToCurrentThreadIfWeakPtr(weak_factory_.GetWeakPtr(), decode_cb)));
}

void D3D11VideoDecoder::Reset(const base::Closure& closure) {
  if (impl_task_runner_->RunsTasksInCurrentSequence()) {
    impl_->Reset(closure);
    return;
  }

  impl_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VideoDecoder::Reset, impl_weak_,
                                BindToCurrentThreadIfWeakPtr(
                                    weak_factory_.GetWeakPtr(), closure)));
}

bool D3D11VideoDecoder::NeedsBitstreamConversion() const {
  // Wrong thread, but it's okay.
  return impl_->NeedsBitstreamConversion();
}

bool D3D11VideoDecoder::CanReadWithoutStalling() const {
  // Wrong thread, but it's okay.
  return impl_->CanReadWithoutStalling();
}

int D3D11VideoDecoder::GetMaxDecodeRequests() const {
  // Wrong thread, but it's okay.
  return impl_->GetMaxDecodeRequests();
}

void D3D11VideoDecoder::SetCreateDeviceCallbackForTesting(
    D3D11CreateDeviceCB callback) {
  create_device_func_ = std::move(callback);
}

void D3D11VideoDecoder::SetWasSupportedReason(
    D3D11VideoNotSupportedReason enum_value) {
  UMA_HISTOGRAM_ENUMERATION("Media.D3D11.WasVideoSupported", enum_value);

  const char* reason = nullptr;
  switch (enum_value) {
    case D3D11VideoNotSupportedReason::kVideoIsSupported:
      reason = "Playback is supported by D3D11VideoDecoder";
      break;
    case D3D11VideoNotSupportedReason::kInsufficientD3D11FeatureLevel:
      reason = "Insufficient D3D11 feature level";
      break;
    case D3D11VideoNotSupportedReason::kProfileNotSupported:
      reason = "Video profile is not supported by D3D11VideoDecoder";
      break;
    case D3D11VideoNotSupportedReason::kCodecNotSupported:
      reason = "H264 is required for D3D11VideoDecoder";
      break;
    case D3D11VideoNotSupportedReason::kZeroCopyNv12Required:
      reason = "Must allow zero-copy NV12 for D3D11VideoDecoder";
      break;
    case D3D11VideoNotSupportedReason::kZeroCopyVideoRequired:
      reason = "Must allow zero-copy video for D3D11VideoDecoder";
      break;
  }

  DVLOG(2) << reason;
  if (media_log_) {
    media_log_->AddEvent(media_log_->CreateStringEvent(
        MediaLogEvent::MEDIA_INFO_LOG_ENTRY, "info", reason));
  }
}

bool D3D11VideoDecoder::IsPotentiallySupported(
    const VideoDecoderConfig& config) {
  // TODO(liberato): All of this could be moved into MojoVideoDecoder, so that
  // it could run on the client side and save the IPC hop.

  // Make sure that we support at least 11.1.
  D3D_FEATURE_LEVEL levels[] = {
      D3D_FEATURE_LEVEL_11_1,
  };
  HRESULT hr = create_device_func_.Run(
      nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, levels, ARRAYSIZE(levels),
      D3D11_SDK_VERSION, nullptr, nullptr, nullptr);

  if (FAILED(hr)) {
    SetWasSupportedReason(
        D3D11VideoNotSupportedReason::kInsufficientD3D11FeatureLevel);
    return false;
  }

  // Must be H264.
  // TODO(tmathmeyer): vp9 should be supported.
  const bool is_h264 = config.profile() >= H264PROFILE_MIN &&
                       config.profile() <= H264PROFILE_MAX;

  if (is_h264) {
    if (config.profile() == H264PROFILE_HIGH10PROFILE) {
      // Must use NV12, which excludes HDR.
      SetWasSupportedReason(D3D11VideoNotSupportedReason::kProfileNotSupported);
      return false;
    }
  } else {
    SetWasSupportedReason(D3D11VideoNotSupportedReason::kCodecNotSupported);
    return false;
  }

  // TODO(liberato): dxva checks IsHDR() in the target colorspace, but we don't
  // have the target colorspace.  It's commented as being for vpx, though, so
  // we skip it here for now.

  // Must allow zero-copy of nv12 textures.
  if (!gpu_preferences_.enable_zero_copy_dxgi_video) {
    SetWasSupportedReason(D3D11VideoNotSupportedReason::kZeroCopyNv12Required);
    return false;
  }

  if (gpu_workarounds_.disable_dxgi_zero_copy_video) {
    SetWasSupportedReason(D3D11VideoNotSupportedReason::kZeroCopyVideoRequired);
    return false;
  }

  SetWasSupportedReason(D3D11VideoNotSupportedReason::kVideoIsSupported);
  return true;
}

}  // namespace media
