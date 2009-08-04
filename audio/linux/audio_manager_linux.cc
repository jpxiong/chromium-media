// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/linux/audio_manager_linux.h"

#include "base/at_exit.h"
#include "base/logging.h"
#include "media/audio/fake_audio_output_stream.h"
#include "media/audio/linux/alsa_output.h"
#include "media/audio/linux/alsa_wrapper.h"

namespace {
AudioManagerLinux* g_audio_manager = NULL;
}  // namespace

// Implementation of AudioManager.
bool AudioManagerLinux::HasAudioDevices() {
  // TODO(ajwong): Make this actually query audio devices.
  return true;
}

AudioOutputStream* AudioManagerLinux::MakeAudioStream(Format format,
                                                      int channels,
                                                      int sample_rate,
                                                      char bits_per_sample) {
  // Early return for testing hook.  Do this before checking for
  // |initialized_|.
  if (format == AudioManager::AUDIO_MOCK) {
    return FakeAudioOutputStream::MakeFakeStream();
  }

  if (!initialized_) {
    return NULL;
  }

  // TODO(ajwong): Do we want to be able to configure the device? default
  // should work correctly for all mono/stereo, but not surround, which needs
  // surround40, surround51, etc.
  //
  // http://0pointer.de/blog/projects/guide-to-sound-apis.html
  AlsaPcmOutputStream* stream =
      new AlsaPcmOutputStream(AlsaPcmOutputStream::kDefaultDevice,
                              format, channels, sample_rate, bits_per_sample,
                              wrapper_.get(), audio_thread_.message_loop());

  // TODO(ajwong): Set up this to clear itself when the stream closes.
  active_streams_[stream] = scoped_refptr<AlsaPcmOutputStream>(stream);
  return stream;
}

AudioManagerLinux::AudioManagerLinux()
    : audio_thread_("AudioThread"),
      initialized_(false) {
}

AudioManagerLinux::~AudioManagerLinux() {
}

void AudioManagerLinux::Init() {
  initialized_ = audio_thread_.Start();
  wrapper_.reset(new AlsaWrapper());
}

void AudioManagerLinux::MuteAll() {
  // TODO(ajwong): Implement.
  NOTIMPLEMENTED();
}

void AudioManagerLinux::UnMuteAll() {
  // TODO(ajwong): Implement.
  NOTIMPLEMENTED();
}

// TODO(ajwong): Collapse this with the windows version.
void DestroyAudioManagerLinux(void* not_used) {
  delete g_audio_manager;
  g_audio_manager = NULL;
}

AudioManager* AudioManager::GetAudioManager() {
  if (!g_audio_manager) {
    g_audio_manager = new AudioManagerLinux();
    g_audio_manager->Init();
    base::AtExitManager::RegisterCallback(&DestroyAudioManagerLinux, NULL);
  }
  return g_audio_manager;
}
