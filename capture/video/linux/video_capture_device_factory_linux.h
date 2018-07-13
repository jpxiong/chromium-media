// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Implementation of a VideoCaptureDeviceFactoryLinux class.

#ifndef MEDIA_CAPTURE_VIDEO_LINUX_VIDEO_CAPTURE_DEVICE_FACTORY_LINUX_H_
#define MEDIA_CAPTURE_VIDEO_LINUX_VIDEO_CAPTURE_DEVICE_FACTORY_LINUX_H_

#include "media/capture/video/video_capture_device_factory.h"

#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "media/capture/video/linux/v4l2_capture_device_impl.h"
#include "media/capture/video_capture_types.h"

namespace media {

// Extension of VideoCaptureDeviceFactory to create and manipulate Linux
// devices.
class CAPTURE_EXPORT VideoCaptureDeviceFactoryLinux
    : public VideoCaptureDeviceFactory {
 public:
  class CAPTURE_EXPORT DeviceProvider {
   public:
    virtual ~DeviceProvider() {}
    virtual void GetDeviceIds(std::vector<std::string>* target_container) = 0;
    virtual std::string GetDeviceModelId(const std::string& device_id) = 0;
    virtual std::string GetDeviceDisplayName(const std::string& device_id) = 0;
  };

  explicit VideoCaptureDeviceFactoryLinux(
      scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner);
  ~VideoCaptureDeviceFactoryLinux() override;

  std::unique_ptr<VideoCaptureDevice> CreateDevice(
      const VideoCaptureDeviceDescriptor& device_descriptor) override;
  void GetDeviceDescriptors(
      VideoCaptureDeviceDescriptors* device_descriptors) override;
  void GetSupportedFormats(
      const VideoCaptureDeviceDescriptor& device_descriptor,
      VideoCaptureFormats* supported_formats) override;

 private:
  bool HasUsableFormats(int fd, uint32_t capabilities);
  std::list<float> GetFrameRateList(int fd,
                                    uint32_t fourcc,
                                    uint32_t width,
                                    uint32_t height);
  void GetSupportedFormatsForV4L2BufferType(
      int fd,
      VideoCaptureFormats* supported_formats);

  scoped_refptr<V4L2CaptureDevice> v4l2_;
  std::unique_ptr<DeviceProvider> device_provider_;
  scoped_refptr<base::SingleThreadTaskRunner> ui_task_runner_;
  DISALLOW_COPY_AND_ASSIGN(VideoCaptureDeviceFactoryLinux);
};

}  // namespace media
#endif  // MEDIA_CAPTURE_VIDEO_LINUX_VIDEO_CAPTURE_DEVICE_FACTORY_LINUX_H_
