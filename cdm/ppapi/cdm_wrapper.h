// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CDM_PPAPI_CDM_WRAPPER_H_
#define MEDIA_CDM_PPAPI_CDM_WRAPPER_H_

#include "base/basictypes.h"
#include "media/cdm/ppapi/api/content_decryption_module.h"
#include "media/cdm/ppapi/cdm_helpers.h"
#include "ppapi/cpp/logging.h"

namespace media {

// CdmWrapper wraps different versions of ContentDecryptionModule interfaces and
// exposes a common interface to the caller.
//
// The caller should call CdmWrapper::Create() to create a CDM instance.
// CdmWrapper will first try to create a CDM instance that supports the latest
// CDM interface (ContentDecryptionModule). If such an instance cannot be
// created (e.g. an older CDM was loaded), CdmWrapper will try to create a CDM
// that supports an older version of CDM interface (e.g.
// ContentDecryptionModule_*). Internally CdmWrapper converts the CdmWrapper
// calls to corresponding ContentDecryptionModule calls.
//
// Note that CdmWrapper interface always reflects the latest state of content
// decryption related PPAPI APIs (e.g. pp::ContentDecryptor_Private).
//
// Since this file is highly templated and default implementations are short
// (just a shim layer in most cases), everything is done in this header file.
class CdmWrapper {
 public:
  static CdmWrapper* Create(const char* key_system,
                            int key_system_size,
                            GetCdmHostFunc get_cdm_host_func,
                            void* user_data);

  virtual ~CdmWrapper() {};

  virtual cdm::Status GenerateKeyRequest(const char* type,
                                         int type_size,
                                         const uint8_t* init_data,
                                         int init_data_size) = 0;
  virtual cdm::Status AddKey(const char* session_id,
                             int session_id_size,
                             const uint8_t* key,
                             int key_size,
                             const uint8_t* key_id,
                             int key_id_size) = 0;
  virtual cdm::Status CancelKeyRequest(const char* session_id,
                                       int session_id_size) = 0;
  virtual void TimerExpired(void* context) = 0;
  virtual cdm::Status Decrypt(const cdm::InputBuffer& encrypted_buffer,
                              cdm::DecryptedBlock* decrypted_buffer) = 0;
  virtual cdm::Status InitializeAudioDecoder(
      const cdm::AudioDecoderConfig& audio_decoder_config) = 0;
  virtual cdm::Status InitializeVideoDecoder(
      const cdm::VideoDecoderConfig& video_decoder_config) = 0;
  virtual void DeinitializeDecoder(cdm::StreamType decoder_type) = 0;
  virtual void ResetDecoder(cdm::StreamType decoder_type) = 0;
  virtual cdm::Status DecryptAndDecodeFrame(
      const cdm::InputBuffer& encrypted_buffer,
      cdm::VideoFrame* video_frame) = 0;
  virtual cdm::Status DecryptAndDecodeSamples(
      const cdm::InputBuffer& encrypted_buffer,
      cdm::AudioFrames* audio_frames) = 0;

 protected:
  CdmWrapper() {};

 private:
  DISALLOW_COPY_AND_ASSIGN(CdmWrapper);
};

// Template class that does the CdmWrapper -> CdmInterface conversion. Default
// implementations are provided. Any methods that need special treatment should
// be specialized.
// TODO(xhwang): Remove CdmInterfaceVersion template parameter after we roll
// CDM.h DEPS.
template <class CdmInterface, int CdmInterfaceVersion>
class CdmWrapperImpl : public CdmWrapper {
 public:
  static CdmWrapper* Create(const char* key_system,
                            int key_system_size,
                            GetCdmHostFunc get_cdm_host_func,
                            void* user_data) {
    void* cdm_instance = ::CreateCdmInstance(CdmInterfaceVersion,
        key_system, key_system_size, get_cdm_host_func, user_data);
    if (!cdm_instance)
      return NULL;

    return new CdmWrapperImpl<CdmInterface, CdmInterfaceVersion>(
        static_cast<CdmInterface*>(cdm_instance));
  }

  virtual ~CdmWrapperImpl() {
    cdm_->Destroy();
  }

  virtual cdm::Status GenerateKeyRequest(const char* type,
                                         int type_size,
                                         const uint8_t* init_data,
                                         int init_data_size) OVERRIDE {
    return cdm_->GenerateKeyRequest(type, type_size, init_data, init_data_size);
  }

  virtual cdm::Status AddKey(const char* session_id,
                             int session_id_size,
                             const uint8_t* key,
                             int key_size,
                             const uint8_t* key_id,
                             int key_id_size) OVERRIDE {
    return cdm_->AddKey(
        session_id, session_id_size, key, key_size, key_id, key_id_size);
  }

  virtual cdm::Status CancelKeyRequest(const char* session_id,
                                       int session_id_size) OVERRIDE {
    return cdm_->CancelKeyRequest(session_id, session_id_size);
  }

  virtual void TimerExpired(void* context) OVERRIDE {
    cdm_->TimerExpired(context);
  }

  virtual cdm::Status Decrypt(const cdm::InputBuffer& encrypted_buffer,
                              cdm::DecryptedBlock* decrypted_buffer) OVERRIDE {
    return cdm_->Decrypt(encrypted_buffer, decrypted_buffer);
  }

  virtual cdm::Status InitializeAudioDecoder(
      const cdm::AudioDecoderConfig& audio_decoder_config) OVERRIDE {
    return cdm_->InitializeAudioDecoder(audio_decoder_config);
  }

  virtual cdm::Status InitializeVideoDecoder(
      const cdm::VideoDecoderConfig& video_decoder_config) OVERRIDE {
    return cdm_->InitializeVideoDecoder(video_decoder_config);
  }

  virtual void DeinitializeDecoder(cdm::StreamType decoder_type) OVERRIDE {
    cdm_->DeinitializeDecoder(decoder_type);
  }

  virtual void ResetDecoder(cdm::StreamType decoder_type) OVERRIDE {
    cdm_->ResetDecoder(decoder_type);
  }

  virtual cdm::Status DecryptAndDecodeFrame(
      const cdm::InputBuffer& encrypted_buffer,
      cdm::VideoFrame* video_frame) OVERRIDE {
    return cdm_->DecryptAndDecodeFrame(encrypted_buffer, video_frame);
  }

  virtual cdm::Status DecryptAndDecodeSamples(
      const cdm::InputBuffer& encrypted_buffer,
      cdm::AudioFrames* audio_frames) OVERRIDE {
    return cdm_->DecryptAndDecodeSamples(encrypted_buffer, audio_frames);
  }

 private:
  CdmWrapperImpl(CdmInterface* cdm) : cdm_(cdm) {
    PP_DCHECK(cdm_);
  }

  CdmInterface* cdm_;

  DISALLOW_COPY_AND_ASSIGN(CdmWrapperImpl);
};

// Specializations for old ContentDecryptionModule interfaces.
// For example:

// template <> cdm::Status CdmAdapterImpl<cdm::ContentDecryptionModule_1>::
//     DecryptAndDecodeSamples(const cdm::InputBuffer& encrypted_buffer,
//                             cdm::AudioFrames* audio_frames) {
//   AudioFramesImpl audio_frames_1;
//   cdm::Status status =
//       cdm_->DecryptAndDecodeSamples(encrypted_buffer, &audio_frames_1);
//   if (status != cdm::kSuccess)
//     return status;
//
//   audio_frames->SetFrameBuffer(audio_frames_1.PassFrameBuffer());
//   audio_frames->SetFormat(cdm::kAudioFormatS16);
//   return cdm::kSuccess;
// }

CdmWrapper* CdmWrapper::Create(const char* key_system,
                               int key_system_size,
                               GetCdmHostFunc get_cdm_host_func,
                               void* user_data) {
  // Try to create the CDM using the latest CDM interface version.
  CdmWrapper* cdm_wrapper =
      CdmWrapperImpl<cdm::ContentDecryptionModule, cdm::kCdmInterfaceVersion>::
          Create(key_system, key_system_size, get_cdm_host_func, user_data);

  // Try to see if the CDM supports older version(s) of CDM interface(s).
  // For example:
  //
  // if (cdm_wrapper)
  //   return cdm_wrapper;
  //
  // cdm_wrapper = CdmWrapperImpl<cdm::ContentDecryptionModule_1>::Create(
  //     key_system, key_system_size, get_cdm_host_func, user_data);

  return cdm_wrapper;
}

}  // namespace media

#endif  // MEDIA_CDM_PPAPI_CDM_WRAPPER_H_
