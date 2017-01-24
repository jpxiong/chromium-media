// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "media/capture/video/linux/camera_facing_chromeos.h"
#include "testing/gtest/include/gtest/gtest.h"

#include <base/files/file_util.h>
#include <base/files/file.h>

namespace media {

namespace {

const char kConfigFileContent[] =
    "camera0.lens_facing=1\ncamera0.sensor_orientation=0\ncamera0.module0.usb_"
    "vid_pid=04f2:b53a\n";
}

TEST(CameraFacingChromeOSTest, ParseSuccessfully) {
  const char file_name[] = "fake_camera_characteristics.conf";
  base::WriteFile(base::FilePath(file_name), kConfigFileContent,
                  sizeof(kConfigFileContent));

  std::string file_name_str(file_name);
  CameraFacingChromeOS camera_facing(file_name_str);
  EXPECT_EQ(VideoFacingMode::MEDIA_VIDEO_FACING_ENVIRONMENT,
            camera_facing.GetCameraFacing(std::string("/dev/video2"),
                                          std::string("04f2:b53a")));
}

TEST(CameraFacingChromeOSTest, ConfigFileNotExist) {
  CameraFacingChromeOS camera_facing(std::string("file_not_exist"));
  EXPECT_EQ(VideoFacingMode::MEDIA_VIDEO_FACING_NONE,
            camera_facing.GetCameraFacing(std::string("/dev/video2"),
                                          std::string("04f2:b53a")));
}

}  // namespace media
