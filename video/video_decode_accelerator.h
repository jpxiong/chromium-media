// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_VIDEO_VIDEO_DECODE_ACCELERATOR_H_
#define MEDIA_VIDEO_VIDEO_DECODE_ACCELERATOR_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/unguessable_token.h"
#include "media/base/bitstream_buffer.h"
#include "media/base/cdm_context.h"
#include "media/base/decoder_buffer.h"
#include "media/base/encryption_scheme.h"
#include "media/base/overlay_info.h"
#include "media/base/video_decoder_config.h"
#include "media/video/picture.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/gpu_memory_buffer.h"

typedef unsigned int GLenum;

namespace base {
class SingleThreadTaskRunner;
}

namespace media {

// Video decoder interface.
// This interface is extended by the various components that ultimately
// implement the backend of PPB_VideoDecoder_Dev.
class MEDIA_EXPORT VideoDecodeAccelerator {
 public:
  // Specification of a decoding profile supported by an decoder.
  // |max_resolution| and |min_resolution| are inclusive.
  struct MEDIA_EXPORT SupportedProfile {
    SupportedProfile();
    ~SupportedProfile();
    VideoCodecProfile profile;
    gfx::Size max_resolution;
    gfx::Size min_resolution;
    bool encrypted_only;
  };
  using SupportedProfiles = std::vector<SupportedProfile>;

  struct MEDIA_EXPORT Capabilities {
    Capabilities();
    Capabilities(const Capabilities& other);
    ~Capabilities();

    std::string AsHumanReadableString() const;

    // Flags that can be associated with a VDA.
    enum Flags {
      NO_FLAGS = 0,

      // Normally, the VDA is required to be able to provide all PictureBuffers
      // to the client via PictureReady(), even if the client does not return
      // any of them via ReusePictureBuffer().  The client is only required to
      // return PictureBuffers when it holds all of them, if it wants to get
      // more decoded output.  See VideoDecoder::CanReadWithoutStalling for
      // more context.
      // If this flag is set, then the VDA does not make this guarantee.  The
      // client must return PictureBuffers to be sure that new frames will be
      // provided via PictureReady.
      NEEDS_ALL_PICTURE_BUFFERS_TO_DECODE = 1 << 0,

      // Whether the VDA supports being configured with an output surface for
      // it to render frames to. For example, SurfaceViews on Android.
      SUPPORTS_EXTERNAL_OUTPUT_SURFACE = 1 << 1,

      // If set, the VDA will use deferred initialization if the config
      // indicates that the client supports it as well.  Refer to
      // NotifyInitializationComplete for more details.
      SUPPORTS_DEFERRED_INITIALIZATION = 1 << 2,

      // If set, video frames will have COPY_REQUIRED flag which will cause
      // an extra texture copy during composition.
      REQUIRES_TEXTURE_COPY = 1 << 3,

      // Whether the VDA supports encrypted streams or not.
      SUPPORTS_ENCRYPTED_STREAMS = 1 << 4,

      // If set the decoder does not require a restart in order to switch to
      // using an external output surface.
      SUPPORTS_SET_EXTERNAL_OUTPUT_SURFACE = 1 << 5,
    };

    SupportedProfiles supported_profiles;
    uint32_t flags;
  };

  // Enumeration of potential errors generated by the API.
  // Note: Keep these in sync with PP_VideoDecodeError_Dev. Also do not
  // rearrange, reuse or remove values as they are used for gathering UMA
  // statistics.
  enum Error {
    // An operation was attempted during an incompatible decoder state.
    ILLEGAL_STATE = 1,
    // Invalid argument was passed to an API method.
    INVALID_ARGUMENT,
    // Encoded input is unreadable.
    UNREADABLE_INPUT,
    // A failure occurred at the browser layer or one of its dependencies.
    // Examples of such failures include GPU hardware failures, GPU driver
    // failures, GPU library failures, browser programming errors, and so on.
    PLATFORM_FAILURE,
    // Largest used enum. This should be adjusted when new errors are added.
    ERROR_MAX = PLATFORM_FAILURE,
  };

  // Config structure contains parameters required for the VDA initialization.
  struct MEDIA_EXPORT Config {
    // Specifies the allocation and handling mode for output PictureBuffers.
    // When set to ALLOCATE, the VDA is expected to allocate backing memory
    // for PictureBuffers at the time of AssignPictureBuffers() call.
    // When set to IMPORT, the VDA will not allocate, but after receiving
    // AssignPictureBuffers() call, it will expect a call to
    // ImportBufferForPicture() for each PictureBuffer before use.
    enum class OutputMode {
      ALLOCATE,
      IMPORT,
    };

    Config();
    Config(const Config& config);

    explicit Config(VideoCodecProfile profile);

    ~Config();

    std::string AsHumanReadableString() const;
    bool is_encrypted() const { return encryption_scheme.is_encrypted(); }

    // The video codec and profile.
    VideoCodecProfile profile = VIDEO_CODEC_PROFILE_UNKNOWN;

    // Whether the stream is encrypted, and, if so, the scheme used.
    EncryptionScheme encryption_scheme;

    // The CDM that the VDA should use to decode encrypted streams. Must be
    // set to a valid ID if |is_encrypted|.
    int cdm_id = CdmContext::kInvalidCdmId;

    // Whether the client supports deferred initialization.
    bool is_deferred_initialization_allowed = false;

    // Optional overlay info available at startup, rather than waiting for the
    // VDA to receive a callback.
    OverlayInfo overlay_info;

    // Coded size of the video frame hint, subject to change.
    gfx::Size initial_expected_coded_size = gfx::Size(320, 240);

    OutputMode output_mode = OutputMode::ALLOCATE;

    // The list of picture buffer formats that the client knows how to use. An
    // empty list means any format is supported.
    std::vector<VideoPixelFormat> supported_output_formats;

    // The H264 SPS and PPS configuration data. Not all clients populate these
    // fields, so they should be parsed from the bitstream instead, if required.
    // Each SPS and PPS is prefixed with the Annex B framing bytes: 0, 0, 0, 1.
    std::vector<uint8_t> sps;
    std::vector<uint8_t> pps;

    // Color space specified by the container.
    VideoColorSpace container_color_space;

    // Target color space.
    // Used as a hint to the decoder. Outputting VideoFrames in this color space
    // may avoid extra conversion steps.
    gfx::ColorSpace target_color_space;

    // HDR metadata specified by the container.
    base::Optional<HDRMetadata> hdr_metadata;
  };

  // Interface for collaborating with picture interface to provide memory for
  // output picture and blitting them. These callbacks will not be made unless
  // Initialize() has returned successfully.
  // This interface is extended by the various layers that relay messages back
  // to the plugin, through the PPP_VideoDecoder_Dev interface the plugin
  // implements.
  class MEDIA_EXPORT Client {
   public:
    // Notify the client that deferred initialization has completed successfully
    // or not.  This is required if and only if deferred initialization is
    // supported by the VDA (see Capabilities), and it is supported by the
    // client (see Config::is_deferred_initialization_allowed), and the initial
    // call to VDA::Initialize returns true.
    // The default implementation is a NOTREACHED, since deferred initialization
    // is not supported by default.
    virtual void NotifyInitializationComplete(bool success);

    // Callback to tell client how many and what size of buffers to provide.
    // Note that the actual count provided through AssignPictureBuffers() can be
    // larger than the value requested.
    // |format| indicates what format the decoded frames will be produced in
    // by the VDA, or PIXEL_FORMAT_UNKNOWN if the underlying platform handles
    // this transparently.
    virtual void ProvidePictureBuffers(uint32_t requested_num_of_buffers,
                                       VideoPixelFormat format,
                                       uint32_t textures_per_buffer,
                                       const gfx::Size& dimensions,
                                       uint32_t texture_target) = 0;

    // Callback to dismiss picture buffer that was assigned earlier.
    virtual void DismissPictureBuffer(int32_t picture_buffer_id) = 0;

    // Callback to deliver decoded pictures ready to be displayed.
    virtual void PictureReady(const Picture& picture) = 0;

    // Callback to notify that decoded has decoded the end of the current
    // bitstream buffer.
    virtual void NotifyEndOfBitstreamBuffer(int32_t bitstream_buffer_id) = 0;

    // Flush completion callback.
    virtual void NotifyFlushDone() = 0;

    // Reset completion callback.
    virtual void NotifyResetDone() = 0;

    // Callback to notify about decoding errors. Note that errors in
    // Initialize() will not be reported here, but will instead be indicated by
    // a false return value there.
    virtual void NotifyError(Error error) = 0;

   protected:
    virtual ~Client() {}
  };

  // Video decoder functions.

  // Initializes the video decoder with specific configuration.  Called once per
  // decoder construction.  This call is synchronous and returns true iff
  // initialization is successful, unless deferred initialization is used.
  //
  // By default, deferred initialization is not used.  However, if Config::
  // is_deferred_initialization_allowed is set by the client, and if
  // Capabilities::Flags::SUPPORTS_DEFERRED_INITIALIZATION is set by the VDA,
  // and if VDA::Initialize returns true, then the client can expect a call to
  // NotifyInitializationComplete with the actual success / failure of
  // initialization.  Note that a return value of false from VDA::Initialize
  // indicates that initialization definitely failed, and no callback is needed.
  //
  // For encrypted video, only deferred initialization is supported and |config|
  // must contain a valid |cdm_id|.
  //
  // Parameters:
  //  |config| contains the initialization parameters.
  //  |client| is the client of this video decoder. Does not take ownership of
  //  |client| which must be valid until Destroy() is called.
  virtual bool Initialize(const Config& config, Client* client) = 0;

  // Decodes given bitstream buffer that contains at most one frame.  Once
  // decoder is done with processing |bitstream_buffer| it will call
  // NotifyEndOfBitstreamBuffer() with the bitstream buffer id.
  // Parameters:
  //  |bitstream_buffer| is the input bitstream that is sent for decoding.
  virtual void Decode(const BitstreamBuffer& bitstream_buffer) = 0;

  // Decodes given decoder buffer that contains at most one frame.  Once
  // decoder is done with processing |buffer| it will call
  // NotifyEndOfBitstreamBuffer() with the bitstream id.
  // Parameters:
  //  |buffer| is the input buffer that is sent for decoding.
  //  |bitstream_id| identifies the buffer for PictureReady() and
  //      NotifyEndOfBitstreamBuffer()
  virtual void Decode(scoped_refptr<DecoderBuffer> buffer,
                      int32_t bitstream_id);

  // Assigns a set of texture-backed picture buffers to the video decoder.
  //
  // Ownership of each picture buffer remains with the client, but the client
  // is not allowed to deallocate the buffer before the DismissPictureBuffer
  // callback has been initiated for a given buffer.
  //
  // Parameters:
  //  |buffers| contains the allocated picture buffers for the output.  Note
  //  that the count of buffers may be larger than the count requested through
  //  the call to Client::ProvidePictureBuffers().
  virtual void AssignPictureBuffers(
      const std::vector<PictureBuffer>& buffers) = 0;

  // Imports |gpu_memory_buffer_handle|, pointing to a buffer in |pixel_format|,
  // as backing memory for picture buffer associated with |picture_buffer_id|.
  // This can only be be used if the VDA has been Initialize()d with
  // config.output_mode = IMPORT, and should be preceded by a call to
  // AssignPictureBuffers() to set up the number of PictureBuffers and their
  // details.
  // The |pixel_format| used here may be different from the |pixel_format|
  // required in ProvidePictureBuffers(). If the buffer cannot be imported an
  // error should be notified via NotifyError().
  // After this call, the VDA becomes the owner of the GpuMemoryBufferHandle,
  // and is responsible for closing it after use, also on import failure.
  virtual void ImportBufferForPicture(
      int32_t picture_buffer_id,
      VideoPixelFormat pixel_format,
      const gfx::GpuMemoryBufferHandle& gpu_memory_buffer_handle);

  // Sends picture buffers to be reused by the decoder. This needs to be called
  // for each buffer that has been processed so that decoder may know onto which
  // picture buffers it can write the output to.
  //
  // Parameters:
  //  |picture_buffer_id| id of the picture buffer that is to be reused.
  virtual void ReusePictureBuffer(int32_t picture_buffer_id) = 0;

  // Flushes the decoder: all pending inputs will be decoded and pictures handed
  // back to the client, followed by NotifyFlushDone() being called on the
  // client.  Can be used to implement "end of stream" notification.
  virtual void Flush() = 0;

  // Resets the decoder: all pending inputs are dropped immediately and the
  // decoder returned to a state ready for further Decode()s, followed by
  // NotifyResetDone() being called on the client.  Can be used to implement
  // "seek". After Flush is called, it is OK to call Reset before receiving
  // NotifyFlushDone() and VDA should cancel the flush. Note NotifyFlushDone()
  // may be on the way to the client. If client gets NotifyFlushDone(), it
  // should be before NotifyResetDone().
  virtual void Reset() = 0;

  // An optional graphics surface that the VDA should render to. For setting
  // an output SurfaceView on Android. Passing |kNoSurfaceID| will clear any
  // previously set surface in favor of an internally generated texture.
  // |routing_token| is an optional AndroidOverlay routing token.  At most one
  // should be non-empty.
  virtual void SetOverlayInfo(const OverlayInfo& overlay_info);

  // Destroys the decoder: all pending inputs are dropped immediately and the
  // component is freed.  This call may asynchornously free system resources,
  // but its client-visible effects are synchronous.  After this method returns
  // no more callbacks will be made on the client.  Deletes |this|
  // unconditionally, so make sure to drop all pointers to it!
  virtual void Destroy() = 0;

  // TO BE CALLED IN THE SAME PROCESS AS THE VDA IMPLEMENTATION ONLY.
  //
  // A decode "task" is a sequence that includes a Decode() call from Client,
  // as well as corresponding callbacks to return the input BitstreamBuffer
  // after use, and the resulting output Picture(s).
  //
  // If the Client can support running these three calls on a separate thread,
  // it may call this method to try to set up the VDA implementation to do so.
  // If the VDA can support this as well, return true, otherwise return false.
  // If true is returned, the client may submit each Decode() call (but no other
  // calls) on |decode_task_runner|, and should then expect that
  // NotifyEndOfBitstreamBuffer() and PictureReady() callbacks may come on
  // |decode_task_runner| as well, called on |decode_client|, instead of client
  // provided to Initialize().
  //
  // This method may be called at any time.
  //
  // NOTE 1: some callbacks may still have to come on the main thread and the
  // Client should handle both callbacks coming on main and |decode_task_runner|
  // thread.
  //
  // NOTE 2: VDA implementations of Decode() must return as soon as possible and
  // never block, as |decode_task_runner| may be a latency critical thread
  // (such as the GPU IO thread).
  //
  // One application of this is offloading the GPU Child thread. In general,
  // calls to VDA in GPU process have to be done on the GPU Child thread, as
  // they may require GL context to be current. However, some VDAs may be able
  // to run decode operations without GL context, which helps reduce latency and
  // offloads the GPU Child thread.
  virtual bool TryToSetupDecodeOnSeparateThread(
      const base::WeakPtr<Client>& decode_client,
      const scoped_refptr<base::SingleThreadTaskRunner>& decode_task_runner);

  // Windows creates a BGRA texture.
  // TODO(dshwang): after moving to D3D11, remove this. crbug.com/438691
  virtual GLenum GetSurfaceInternalFormat() const;

 protected:
  // Do not delete directly; use Destroy() or own it with a scoped_ptr, which
  // will Destroy() it properly by default.
  virtual ~VideoDecodeAccelerator();
};

}  // namespace media

namespace std {

// Specialize std::default_delete so that
// std::unique_ptr<VideoDecodeAccelerator> uses "Destroy()" instead of trying to
// use the destructor.
template <>
struct MEDIA_EXPORT default_delete<media::VideoDecodeAccelerator> {
  void operator()(media::VideoDecodeAccelerator* vda) const;
};

}  // namespace std

#endif  // MEDIA_VIDEO_VIDEO_DECODE_ACCELERATOR_H_
