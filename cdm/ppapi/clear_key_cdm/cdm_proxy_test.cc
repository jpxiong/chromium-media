// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cdm/ppapi/clear_key_cdm/cdm_proxy_test.h"

#include "base/logging.h"
#include "media/cdm/ppapi/clear_key_cdm/cdm_host_proxy.h"

namespace media {

CdmProxyTest::CdmProxyTest(CdmHostProxy* cdm_host_proxy)
    : cdm_host_proxy_(cdm_host_proxy) {}

CdmProxyTest::~CdmProxyTest() {}

void CdmProxyTest::Run(CompletionCB completion_cb) {
  DVLOG(1) << __func__;
  completion_cb_ = std::move(completion_cb);

  cdm_proxy_ = cdm_host_proxy_->CreateCdmProxy();
  if (!cdm_proxy_)
    OnTestComplete(false);

  cdm_proxy_->Initialize(this);
}

void CdmProxyTest::OnTestComplete(bool success) {
  DVLOG(1) << __func__ << ": success = " << success;
  std::move(completion_cb_).Run(success);
}

void CdmProxyTest::OnInitialized(Status status,
                                 Protocol protocol,
                                 uint32_t crypto_session_id) {
  DVLOG(1) << __func__ << ": status = " << status;
  // Ignore the |status| for now.
  // TODO(xhwang): Add a test CdmProxy and test all APIs.
  OnTestComplete(true);
}
void CdmProxyTest::OnProcessed(Status status,
                               const uint8_t* output_data,
                               uint32_t output_data_size) {
  DVLOG(1) << __func__ << ": status = " << status;
  NOTREACHED();
}

void CdmProxyTest::OnMediaCryptoSessionCreated(Status status,
                                               uint32_t crypto_session_id,
                                               uint64_t output_data) {
  DVLOG(1) << __func__ << ": status = " << status;
  NOTREACHED();
}

void CdmProxyTest::NotifyHardwareReset() {
  DVLOG(1) << __func__;
  NOTREACHED();
}

}  // namespace media