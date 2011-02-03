// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/yuv_row.h"

extern "C" {

#define RGBY(i) { \
  static_cast<int16>(1.164 * 64 * (i - 16) + 0.5), \
  static_cast<int16>(1.164 * 64 * (i - 16) + 0.5), \
  static_cast<int16>(1.164 * 64 * (i - 16) + 0.5), \
  0 \
}

#define RGBU(i) { \
  static_cast<int16>(2.018 * 64 * (i - 128) + 0.5), \
  static_cast<int16>(-0.391 * 64 * (i - 128) + 0.5), \
  0, \
  static_cast<int16>(256 * 64 - 1) \
}

#define RGBV(i) { \
  0, \
  static_cast<int16>(-0.813 * 64 * (i - 128) + 0.5), \
  static_cast<int16>(1.596 * 64 * (i - 128) + 0.5), \
  0 \
}

#define YUVR(i) { \
  static_cast<int16>(0.254 * 64 * i + 0.5),  \
  0,                                         \
  static_cast<int16>(-0.148 * 64 * i + 0.5), \
  static_cast<int16>(0.439 * 64 * i + 0.5),  \
}

#define YUVG(i) { \
  static_cast<int16>(0.504 * 64 * i + 0.5),  \
  0,                                         \
  static_cast<int16>(-0.291 * 64 * i + 0.5), \
  static_cast<int16>(-0.368 * 64 * i + 0.5), \
}

#define YUVB(i) { \
  static_cast<int16>(0.098 * 64 * i + 0.5),  \
  0,                                         \
  static_cast<int16>(0.439 * 64 * i + 0.5),  \
  static_cast<int16>(-0.071 * 64 * i + 0.5), \
}

SIMD_ALIGNED(int16 kCoefficientsRgbY[256 * 3][4]) = {
  RGBY(0x00), RGBY(0x01), RGBY(0x02), RGBY(0x03),
  RGBY(0x04), RGBY(0x05), RGBY(0x06), RGBY(0x07),
  RGBY(0x08), RGBY(0x09), RGBY(0x0A), RGBY(0x0B),
  RGBY(0x0C), RGBY(0x0D), RGBY(0x0E), RGBY(0x0F),
  RGBY(0x10), RGBY(0x11), RGBY(0x12), RGBY(0x13),
  RGBY(0x14), RGBY(0x15), RGBY(0x16), RGBY(0x17),
  RGBY(0x18), RGBY(0x19), RGBY(0x1A), RGBY(0x1B),
  RGBY(0x1C), RGBY(0x1D), RGBY(0x1E), RGBY(0x1F),
  RGBY(0x20), RGBY(0x21), RGBY(0x22), RGBY(0x23),
  RGBY(0x24), RGBY(0x25), RGBY(0x26), RGBY(0x27),
  RGBY(0x28), RGBY(0x29), RGBY(0x2A), RGBY(0x2B),
  RGBY(0x2C), RGBY(0x2D), RGBY(0x2E), RGBY(0x2F),
  RGBY(0x30), RGBY(0x31), RGBY(0x32), RGBY(0x33),
  RGBY(0x34), RGBY(0x35), RGBY(0x36), RGBY(0x37),
  RGBY(0x38), RGBY(0x39), RGBY(0x3A), RGBY(0x3B),
  RGBY(0x3C), RGBY(0x3D), RGBY(0x3E), RGBY(0x3F),
  RGBY(0x40), RGBY(0x41), RGBY(0x42), RGBY(0x43),
  RGBY(0x44), RGBY(0x45), RGBY(0x46), RGBY(0x47),
  RGBY(0x48), RGBY(0x49), RGBY(0x4A), RGBY(0x4B),
  RGBY(0x4C), RGBY(0x4D), RGBY(0x4E), RGBY(0x4F),
  RGBY(0x50), RGBY(0x51), RGBY(0x52), RGBY(0x53),
  RGBY(0x54), RGBY(0x55), RGBY(0x56), RGBY(0x57),
  RGBY(0x58), RGBY(0x59), RGBY(0x5A), RGBY(0x5B),
  RGBY(0x5C), RGBY(0x5D), RGBY(0x5E), RGBY(0x5F),
  RGBY(0x60), RGBY(0x61), RGBY(0x62), RGBY(0x63),
  RGBY(0x64), RGBY(0x65), RGBY(0x66), RGBY(0x67),
  RGBY(0x68), RGBY(0x69), RGBY(0x6A), RGBY(0x6B),
  RGBY(0x6C), RGBY(0x6D), RGBY(0x6E), RGBY(0x6F),
  RGBY(0x70), RGBY(0x71), RGBY(0x72), RGBY(0x73),
  RGBY(0x74), RGBY(0x75), RGBY(0x76), RGBY(0x77),
  RGBY(0x78), RGBY(0x79), RGBY(0x7A), RGBY(0x7B),
  RGBY(0x7C), RGBY(0x7D), RGBY(0x7E), RGBY(0x7F),
  RGBY(0x80), RGBY(0x81), RGBY(0x82), RGBY(0x83),
  RGBY(0x84), RGBY(0x85), RGBY(0x86), RGBY(0x87),
  RGBY(0x88), RGBY(0x89), RGBY(0x8A), RGBY(0x8B),
  RGBY(0x8C), RGBY(0x8D), RGBY(0x8E), RGBY(0x8F),
  RGBY(0x90), RGBY(0x91), RGBY(0x92), RGBY(0x93),
  RGBY(0x94), RGBY(0x95), RGBY(0x96), RGBY(0x97),
  RGBY(0x98), RGBY(0x99), RGBY(0x9A), RGBY(0x9B),
  RGBY(0x9C), RGBY(0x9D), RGBY(0x9E), RGBY(0x9F),
  RGBY(0xA0), RGBY(0xA1), RGBY(0xA2), RGBY(0xA3),
  RGBY(0xA4), RGBY(0xA5), RGBY(0xA6), RGBY(0xA7),
  RGBY(0xA8), RGBY(0xA9), RGBY(0xAA), RGBY(0xAB),
  RGBY(0xAC), RGBY(0xAD), RGBY(0xAE), RGBY(0xAF),
  RGBY(0xB0), RGBY(0xB1), RGBY(0xB2), RGBY(0xB3),
  RGBY(0xB4), RGBY(0xB5), RGBY(0xB6), RGBY(0xB7),
  RGBY(0xB8), RGBY(0xB9), RGBY(0xBA), RGBY(0xBB),
  RGBY(0xBC), RGBY(0xBD), RGBY(0xBE), RGBY(0xBF),
  RGBY(0xC0), RGBY(0xC1), RGBY(0xC2), RGBY(0xC3),
  RGBY(0xC4), RGBY(0xC5), RGBY(0xC6), RGBY(0xC7),
  RGBY(0xC8), RGBY(0xC9), RGBY(0xCA), RGBY(0xCB),
  RGBY(0xCC), RGBY(0xCD), RGBY(0xCE), RGBY(0xCF),
  RGBY(0xD0), RGBY(0xD1), RGBY(0xD2), RGBY(0xD3),
  RGBY(0xD4), RGBY(0xD5), RGBY(0xD6), RGBY(0xD7),
  RGBY(0xD8), RGBY(0xD9), RGBY(0xDA), RGBY(0xDB),
  RGBY(0xDC), RGBY(0xDD), RGBY(0xDE), RGBY(0xDF),
  RGBY(0xE0), RGBY(0xE1), RGBY(0xE2), RGBY(0xE3),
  RGBY(0xE4), RGBY(0xE5), RGBY(0xE6), RGBY(0xE7),
  RGBY(0xE8), RGBY(0xE9), RGBY(0xEA), RGBY(0xEB),
  RGBY(0xEC), RGBY(0xED), RGBY(0xEE), RGBY(0xEF),
  RGBY(0xF0), RGBY(0xF1), RGBY(0xF2), RGBY(0xF3),
  RGBY(0xF4), RGBY(0xF5), RGBY(0xF6), RGBY(0xF7),
  RGBY(0xF8), RGBY(0xF9), RGBY(0xFA), RGBY(0xFB),
  RGBY(0xFC), RGBY(0xFD), RGBY(0xFE), RGBY(0xFF),

  // Chroma U table.
  RGBU(0x00), RGBU(0x01), RGBU(0x02), RGBU(0x03),
  RGBU(0x04), RGBU(0x05), RGBU(0x06), RGBU(0x07),
  RGBU(0x08), RGBU(0x09), RGBU(0x0A), RGBU(0x0B),
  RGBU(0x0C), RGBU(0x0D), RGBU(0x0E), RGBU(0x0F),
  RGBU(0x10), RGBU(0x11), RGBU(0x12), RGBU(0x13),
  RGBU(0x14), RGBU(0x15), RGBU(0x16), RGBU(0x17),
  RGBU(0x18), RGBU(0x19), RGBU(0x1A), RGBU(0x1B),
  RGBU(0x1C), RGBU(0x1D), RGBU(0x1E), RGBU(0x1F),
  RGBU(0x20), RGBU(0x21), RGBU(0x22), RGBU(0x23),
  RGBU(0x24), RGBU(0x25), RGBU(0x26), RGBU(0x27),
  RGBU(0x28), RGBU(0x29), RGBU(0x2A), RGBU(0x2B),
  RGBU(0x2C), RGBU(0x2D), RGBU(0x2E), RGBU(0x2F),
  RGBU(0x30), RGBU(0x31), RGBU(0x32), RGBU(0x33),
  RGBU(0x34), RGBU(0x35), RGBU(0x36), RGBU(0x37),
  RGBU(0x38), RGBU(0x39), RGBU(0x3A), RGBU(0x3B),
  RGBU(0x3C), RGBU(0x3D), RGBU(0x3E), RGBU(0x3F),
  RGBU(0x40), RGBU(0x41), RGBU(0x42), RGBU(0x43),
  RGBU(0x44), RGBU(0x45), RGBU(0x46), RGBU(0x47),
  RGBU(0x48), RGBU(0x49), RGBU(0x4A), RGBU(0x4B),
  RGBU(0x4C), RGBU(0x4D), RGBU(0x4E), RGBU(0x4F),
  RGBU(0x50), RGBU(0x51), RGBU(0x52), RGBU(0x53),
  RGBU(0x54), RGBU(0x55), RGBU(0x56), RGBU(0x57),
  RGBU(0x58), RGBU(0x59), RGBU(0x5A), RGBU(0x5B),
  RGBU(0x5C), RGBU(0x5D), RGBU(0x5E), RGBU(0x5F),
  RGBU(0x60), RGBU(0x61), RGBU(0x62), RGBU(0x63),
  RGBU(0x64), RGBU(0x65), RGBU(0x66), RGBU(0x67),
  RGBU(0x68), RGBU(0x69), RGBU(0x6A), RGBU(0x6B),
  RGBU(0x6C), RGBU(0x6D), RGBU(0x6E), RGBU(0x6F),
  RGBU(0x70), RGBU(0x71), RGBU(0x72), RGBU(0x73),
  RGBU(0x74), RGBU(0x75), RGBU(0x76), RGBU(0x77),
  RGBU(0x78), RGBU(0x79), RGBU(0x7A), RGBU(0x7B),
  RGBU(0x7C), RGBU(0x7D), RGBU(0x7E), RGBU(0x7F),
  RGBU(0x80), RGBU(0x81), RGBU(0x82), RGBU(0x83),
  RGBU(0x84), RGBU(0x85), RGBU(0x86), RGBU(0x87),
  RGBU(0x88), RGBU(0x89), RGBU(0x8A), RGBU(0x8B),
  RGBU(0x8C), RGBU(0x8D), RGBU(0x8E), RGBU(0x8F),
  RGBU(0x90), RGBU(0x91), RGBU(0x92), RGBU(0x93),
  RGBU(0x94), RGBU(0x95), RGBU(0x96), RGBU(0x97),
  RGBU(0x98), RGBU(0x99), RGBU(0x9A), RGBU(0x9B),
  RGBU(0x9C), RGBU(0x9D), RGBU(0x9E), RGBU(0x9F),
  RGBU(0xA0), RGBU(0xA1), RGBU(0xA2), RGBU(0xA3),
  RGBU(0xA4), RGBU(0xA5), RGBU(0xA6), RGBU(0xA7),
  RGBU(0xA8), RGBU(0xA9), RGBU(0xAA), RGBU(0xAB),
  RGBU(0xAC), RGBU(0xAD), RGBU(0xAE), RGBU(0xAF),
  RGBU(0xB0), RGBU(0xB1), RGBU(0xB2), RGBU(0xB3),
  RGBU(0xB4), RGBU(0xB5), RGBU(0xB6), RGBU(0xB7),
  RGBU(0xB8), RGBU(0xB9), RGBU(0xBA), RGBU(0xBB),
  RGBU(0xBC), RGBU(0xBD), RGBU(0xBE), RGBU(0xBF),
  RGBU(0xC0), RGBU(0xC1), RGBU(0xC2), RGBU(0xC3),
  RGBU(0xC4), RGBU(0xC5), RGBU(0xC6), RGBU(0xC7),
  RGBU(0xC8), RGBU(0xC9), RGBU(0xCA), RGBU(0xCB),
  RGBU(0xCC), RGBU(0xCD), RGBU(0xCE), RGBU(0xCF),
  RGBU(0xD0), RGBU(0xD1), RGBU(0xD2), RGBU(0xD3),
  RGBU(0xD4), RGBU(0xD5), RGBU(0xD6), RGBU(0xD7),
  RGBU(0xD8), RGBU(0xD9), RGBU(0xDA), RGBU(0xDB),
  RGBU(0xDC), RGBU(0xDD), RGBU(0xDE), RGBU(0xDF),
  RGBU(0xE0), RGBU(0xE1), RGBU(0xE2), RGBU(0xE3),
  RGBU(0xE4), RGBU(0xE5), RGBU(0xE6), RGBU(0xE7),
  RGBU(0xE8), RGBU(0xE9), RGBU(0xEA), RGBU(0xEB),
  RGBU(0xEC), RGBU(0xED), RGBU(0xEE), RGBU(0xEF),
  RGBU(0xF0), RGBU(0xF1), RGBU(0xF2), RGBU(0xF3),
  RGBU(0xF4), RGBU(0xF5), RGBU(0xF6), RGBU(0xF7),
  RGBU(0xF8), RGBU(0xF9), RGBU(0xFA), RGBU(0xFB),
  RGBU(0xFC), RGBU(0xFD), RGBU(0xFE), RGBU(0xFF),

  // Chroma V table.
  RGBV(0x00), RGBV(0x01), RGBV(0x02), RGBV(0x03),
  RGBV(0x04), RGBV(0x05), RGBV(0x06), RGBV(0x07),
  RGBV(0x08), RGBV(0x09), RGBV(0x0A), RGBV(0x0B),
  RGBV(0x0C), RGBV(0x0D), RGBV(0x0E), RGBV(0x0F),
  RGBV(0x10), RGBV(0x11), RGBV(0x12), RGBV(0x13),
  RGBV(0x14), RGBV(0x15), RGBV(0x16), RGBV(0x17),
  RGBV(0x18), RGBV(0x19), RGBV(0x1A), RGBV(0x1B),
  RGBV(0x1C), RGBV(0x1D), RGBV(0x1E), RGBV(0x1F),
  RGBV(0x20), RGBV(0x21), RGBV(0x22), RGBV(0x23),
  RGBV(0x24), RGBV(0x25), RGBV(0x26), RGBV(0x27),
  RGBV(0x28), RGBV(0x29), RGBV(0x2A), RGBV(0x2B),
  RGBV(0x2C), RGBV(0x2D), RGBV(0x2E), RGBV(0x2F),
  RGBV(0x30), RGBV(0x31), RGBV(0x32), RGBV(0x33),
  RGBV(0x34), RGBV(0x35), RGBV(0x36), RGBV(0x37),
  RGBV(0x38), RGBV(0x39), RGBV(0x3A), RGBV(0x3B),
  RGBV(0x3C), RGBV(0x3D), RGBV(0x3E), RGBV(0x3F),
  RGBV(0x40), RGBV(0x41), RGBV(0x42), RGBV(0x43),
  RGBV(0x44), RGBV(0x45), RGBV(0x46), RGBV(0x47),
  RGBV(0x48), RGBV(0x49), RGBV(0x4A), RGBV(0x4B),
  RGBV(0x4C), RGBV(0x4D), RGBV(0x4E), RGBV(0x4F),
  RGBV(0x50), RGBV(0x51), RGBV(0x52), RGBV(0x53),
  RGBV(0x54), RGBV(0x55), RGBV(0x56), RGBV(0x57),
  RGBV(0x58), RGBV(0x59), RGBV(0x5A), RGBV(0x5B),
  RGBV(0x5C), RGBV(0x5D), RGBV(0x5E), RGBV(0x5F),
  RGBV(0x60), RGBV(0x61), RGBV(0x62), RGBV(0x63),
  RGBV(0x64), RGBV(0x65), RGBV(0x66), RGBV(0x67),
  RGBV(0x68), RGBV(0x69), RGBV(0x6A), RGBV(0x6B),
  RGBV(0x6C), RGBV(0x6D), RGBV(0x6E), RGBV(0x6F),
  RGBV(0x70), RGBV(0x71), RGBV(0x72), RGBV(0x73),
  RGBV(0x74), RGBV(0x75), RGBV(0x76), RGBV(0x77),
  RGBV(0x78), RGBV(0x79), RGBV(0x7A), RGBV(0x7B),
  RGBV(0x7C), RGBV(0x7D), RGBV(0x7E), RGBV(0x7F),
  RGBV(0x80), RGBV(0x81), RGBV(0x82), RGBV(0x83),
  RGBV(0x84), RGBV(0x85), RGBV(0x86), RGBV(0x87),
  RGBV(0x88), RGBV(0x89), RGBV(0x8A), RGBV(0x8B),
  RGBV(0x8C), RGBV(0x8D), RGBV(0x8E), RGBV(0x8F),
  RGBV(0x90), RGBV(0x91), RGBV(0x92), RGBV(0x93),
  RGBV(0x94), RGBV(0x95), RGBV(0x96), RGBV(0x97),
  RGBV(0x98), RGBV(0x99), RGBV(0x9A), RGBV(0x9B),
  RGBV(0x9C), RGBV(0x9D), RGBV(0x9E), RGBV(0x9F),
  RGBV(0xA0), RGBV(0xA1), RGBV(0xA2), RGBV(0xA3),
  RGBV(0xA4), RGBV(0xA5), RGBV(0xA6), RGBV(0xA7),
  RGBV(0xA8), RGBV(0xA9), RGBV(0xAA), RGBV(0xAB),
  RGBV(0xAC), RGBV(0xAD), RGBV(0xAE), RGBV(0xAF),
  RGBV(0xB0), RGBV(0xB1), RGBV(0xB2), RGBV(0xB3),
  RGBV(0xB4), RGBV(0xB5), RGBV(0xB6), RGBV(0xB7),
  RGBV(0xB8), RGBV(0xB9), RGBV(0xBA), RGBV(0xBB),
  RGBV(0xBC), RGBV(0xBD), RGBV(0xBE), RGBV(0xBF),
  RGBV(0xC0), RGBV(0xC1), RGBV(0xC2), RGBV(0xC3),
  RGBV(0xC4), RGBV(0xC5), RGBV(0xC6), RGBV(0xC7),
  RGBV(0xC8), RGBV(0xC9), RGBV(0xCA), RGBV(0xCB),
  RGBV(0xCC), RGBV(0xCD), RGBV(0xCE), RGBV(0xCF),
  RGBV(0xD0), RGBV(0xD1), RGBV(0xD2), RGBV(0xD3),
  RGBV(0xD4), RGBV(0xD5), RGBV(0xD6), RGBV(0xD7),
  RGBV(0xD8), RGBV(0xD9), RGBV(0xDA), RGBV(0xDB),
  RGBV(0xDC), RGBV(0xDD), RGBV(0xDE), RGBV(0xDF),
  RGBV(0xE0), RGBV(0xE1), RGBV(0xE2), RGBV(0xE3),
  RGBV(0xE4), RGBV(0xE5), RGBV(0xE6), RGBV(0xE7),
  RGBV(0xE8), RGBV(0xE9), RGBV(0xEA), RGBV(0xEB),
  RGBV(0xEC), RGBV(0xED), RGBV(0xEE), RGBV(0xEF),
  RGBV(0xF0), RGBV(0xF1), RGBV(0xF2), RGBV(0xF3),
  RGBV(0xF4), RGBV(0xF5), RGBV(0xF6), RGBV(0xF7),
  RGBV(0xF8), RGBV(0xF9), RGBV(0xFA), RGBV(0xFB),
  RGBV(0xFC), RGBV(0xFD), RGBV(0xFE), RGBV(0xFF),
};

SIMD_ALIGNED(int16 kCoefficientsYuvR[256 * 3][4]) = {
  // R table.
  YUVR(0x00), YUVR(0x01), YUVR(0x02), YUVR(0x03),
  YUVR(0x04), YUVR(0x05), YUVR(0x06), YUVR(0x07),
  YUVR(0x08), YUVR(0x09), YUVR(0x0A), YUVR(0x0B),
  YUVR(0x0C), YUVR(0x0D), YUVR(0x0E), YUVR(0x0F),
  YUVR(0x10), YUVR(0x11), YUVR(0x12), YUVR(0x13),
  YUVR(0x14), YUVR(0x15), YUVR(0x16), YUVR(0x17),
  YUVR(0x18), YUVR(0x19), YUVR(0x1A), YUVR(0x1B),
  YUVR(0x1C), YUVR(0x1D), YUVR(0x1E), YUVR(0x1F),
  YUVR(0x20), YUVR(0x21), YUVR(0x22), YUVR(0x23),
  YUVR(0x24), YUVR(0x25), YUVR(0x26), YUVR(0x27),
  YUVR(0x28), YUVR(0x29), YUVR(0x2A), YUVR(0x2B),
  YUVR(0x2C), YUVR(0x2D), YUVR(0x2E), YUVR(0x2F),
  YUVR(0x30), YUVR(0x31), YUVR(0x32), YUVR(0x33),
  YUVR(0x34), YUVR(0x35), YUVR(0x36), YUVR(0x37),
  YUVR(0x38), YUVR(0x39), YUVR(0x3A), YUVR(0x3B),
  YUVR(0x3C), YUVR(0x3D), YUVR(0x3E), YUVR(0x3F),
  YUVR(0x40), YUVR(0x41), YUVR(0x42), YUVR(0x43),
  YUVR(0x44), YUVR(0x45), YUVR(0x46), YUVR(0x47),
  YUVR(0x48), YUVR(0x49), YUVR(0x4A), YUVR(0x4B),
  YUVR(0x4C), YUVR(0x4D), YUVR(0x4E), YUVR(0x4F),
  YUVR(0x50), YUVR(0x51), YUVR(0x52), YUVR(0x53),
  YUVR(0x54), YUVR(0x55), YUVR(0x56), YUVR(0x57),
  YUVR(0x58), YUVR(0x59), YUVR(0x5A), YUVR(0x5B),
  YUVR(0x5C), YUVR(0x5D), YUVR(0x5E), YUVR(0x5F),
  YUVR(0x60), YUVR(0x61), YUVR(0x62), YUVR(0x63),
  YUVR(0x64), YUVR(0x65), YUVR(0x66), YUVR(0x67),
  YUVR(0x68), YUVR(0x69), YUVR(0x6A), YUVR(0x6B),
  YUVR(0x6C), YUVR(0x6D), YUVR(0x6E), YUVR(0x6F),
  YUVR(0x70), YUVR(0x71), YUVR(0x72), YUVR(0x73),
  YUVR(0x74), YUVR(0x75), YUVR(0x76), YUVR(0x77),
  YUVR(0x78), YUVR(0x79), YUVR(0x7A), YUVR(0x7B),
  YUVR(0x7C), YUVR(0x7D), YUVR(0x7E), YUVR(0x7F),
  YUVR(0x80), YUVR(0x81), YUVR(0x82), YUVR(0x83),
  YUVR(0x84), YUVR(0x85), YUVR(0x86), YUVR(0x87),
  YUVR(0x88), YUVR(0x89), YUVR(0x8A), YUVR(0x8B),
  YUVR(0x8C), YUVR(0x8D), YUVR(0x8E), YUVR(0x8F),
  YUVR(0x90), YUVR(0x91), YUVR(0x92), YUVR(0x93),
  YUVR(0x94), YUVR(0x95), YUVR(0x96), YUVR(0x97),
  YUVR(0x98), YUVR(0x99), YUVR(0x9A), YUVR(0x9B),
  YUVR(0x9C), YUVR(0x9D), YUVR(0x9E), YUVR(0x9F),
  YUVR(0xA0), YUVR(0xA1), YUVR(0xA2), YUVR(0xA3),
  YUVR(0xA4), YUVR(0xA5), YUVR(0xA6), YUVR(0xA7),
  YUVR(0xA8), YUVR(0xA9), YUVR(0xAA), YUVR(0xAB),
  YUVR(0xAC), YUVR(0xAD), YUVR(0xAE), YUVR(0xAF),
  YUVR(0xB0), YUVR(0xB1), YUVR(0xB2), YUVR(0xB3),
  YUVR(0xB4), YUVR(0xB5), YUVR(0xB6), YUVR(0xB7),
  YUVR(0xB8), YUVR(0xB9), YUVR(0xBA), YUVR(0xBB),
  YUVR(0xBC), YUVR(0xBD), YUVR(0xBE), YUVR(0xBF),
  YUVR(0xC0), YUVR(0xC1), YUVR(0xC2), YUVR(0xC3),
  YUVR(0xC4), YUVR(0xC5), YUVR(0xC6), YUVR(0xC7),
  YUVR(0xC8), YUVR(0xC9), YUVR(0xCA), YUVR(0xCB),
  YUVR(0xCC), YUVR(0xCD), YUVR(0xCE), YUVR(0xCF),
  YUVR(0xD0), YUVR(0xD1), YUVR(0xD2), YUVR(0xD3),
  YUVR(0xD4), YUVR(0xD5), YUVR(0xD6), YUVR(0xD7),
  YUVR(0xD8), YUVR(0xD9), YUVR(0xDA), YUVR(0xDB),
  YUVR(0xDC), YUVR(0xDD), YUVR(0xDE), YUVR(0xDF),
  YUVR(0xE0), YUVR(0xE1), YUVR(0xE2), YUVR(0xE3),
  YUVR(0xE4), YUVR(0xE5), YUVR(0xE6), YUVR(0xE7),
  YUVR(0xE8), YUVR(0xE9), YUVR(0xEA), YUVR(0xEB),
  YUVR(0xEC), YUVR(0xED), YUVR(0xEE), YUVR(0xEF),
  YUVR(0xF0), YUVR(0xF1), YUVR(0xF2), YUVR(0xF3),
  YUVR(0xF4), YUVR(0xF5), YUVR(0xF6), YUVR(0xF7),
  YUVR(0xF8), YUVR(0xF9), YUVR(0xFA), YUVR(0xFB),
  YUVR(0xFC), YUVR(0xFD), YUVR(0xFE), YUVR(0xFF),

  // G table.
  YUVG(0x00), YUVG(0x01), YUVG(0x02), YUVG(0x03),
  YUVG(0x04), YUVG(0x05), YUVG(0x06), YUVG(0x07),
  YUVG(0x08), YUVG(0x09), YUVG(0x0A), YUVG(0x0B),
  YUVG(0x0C), YUVG(0x0D), YUVG(0x0E), YUVG(0x0F),
  YUVG(0x10), YUVG(0x11), YUVG(0x12), YUVG(0x13),
  YUVG(0x14), YUVG(0x15), YUVG(0x16), YUVG(0x17),
  YUVG(0x18), YUVG(0x19), YUVG(0x1A), YUVG(0x1B),
  YUVG(0x1C), YUVG(0x1D), YUVG(0x1E), YUVG(0x1F),
  YUVG(0x20), YUVG(0x21), YUVG(0x22), YUVG(0x23),
  YUVG(0x24), YUVG(0x25), YUVG(0x26), YUVG(0x27),
  YUVG(0x28), YUVG(0x29), YUVG(0x2A), YUVG(0x2B),
  YUVG(0x2C), YUVG(0x2D), YUVG(0x2E), YUVG(0x2F),
  YUVG(0x30), YUVG(0x31), YUVG(0x32), YUVG(0x33),
  YUVG(0x34), YUVG(0x35), YUVG(0x36), YUVG(0x37),
  YUVG(0x38), YUVG(0x39), YUVG(0x3A), YUVG(0x3B),
  YUVG(0x3C), YUVG(0x3D), YUVG(0x3E), YUVG(0x3F),
  YUVG(0x40), YUVG(0x41), YUVG(0x42), YUVG(0x43),
  YUVG(0x44), YUVG(0x45), YUVG(0x46), YUVG(0x47),
  YUVG(0x48), YUVG(0x49), YUVG(0x4A), YUVG(0x4B),
  YUVG(0x4C), YUVG(0x4D), YUVG(0x4E), YUVG(0x4F),
  YUVG(0x50), YUVG(0x51), YUVG(0x52), YUVG(0x53),
  YUVG(0x54), YUVG(0x55), YUVG(0x56), YUVG(0x57),
  YUVG(0x58), YUVG(0x59), YUVG(0x5A), YUVG(0x5B),
  YUVG(0x5C), YUVG(0x5D), YUVG(0x5E), YUVG(0x5F),
  YUVG(0x60), YUVG(0x61), YUVG(0x62), YUVG(0x63),
  YUVG(0x64), YUVG(0x65), YUVG(0x66), YUVG(0x67),
  YUVG(0x68), YUVG(0x69), YUVG(0x6A), YUVG(0x6B),
  YUVG(0x6C), YUVG(0x6D), YUVG(0x6E), YUVG(0x6F),
  YUVG(0x70), YUVG(0x71), YUVG(0x72), YUVG(0x73),
  YUVG(0x74), YUVG(0x75), YUVG(0x76), YUVG(0x77),
  YUVG(0x78), YUVG(0x79), YUVG(0x7A), YUVG(0x7B),
  YUVG(0x7C), YUVG(0x7D), YUVG(0x7E), YUVG(0x7F),
  YUVG(0x80), YUVG(0x81), YUVG(0x82), YUVG(0x83),
  YUVG(0x84), YUVG(0x85), YUVG(0x86), YUVG(0x87),
  YUVG(0x88), YUVG(0x89), YUVG(0x8A), YUVG(0x8B),
  YUVG(0x8C), YUVG(0x8D), YUVG(0x8E), YUVG(0x8F),
  YUVG(0x90), YUVG(0x91), YUVG(0x92), YUVG(0x93),
  YUVG(0x94), YUVG(0x95), YUVG(0x96), YUVG(0x97),
  YUVG(0x98), YUVG(0x99), YUVG(0x9A), YUVG(0x9B),
  YUVG(0x9C), YUVG(0x9D), YUVG(0x9E), YUVG(0x9F),
  YUVG(0xA0), YUVG(0xA1), YUVG(0xA2), YUVG(0xA3),
  YUVG(0xA4), YUVG(0xA5), YUVG(0xA6), YUVG(0xA7),
  YUVG(0xA8), YUVG(0xA9), YUVG(0xAA), YUVG(0xAB),
  YUVG(0xAC), YUVG(0xAD), YUVG(0xAE), YUVG(0xAF),
  YUVG(0xB0), YUVG(0xB1), YUVG(0xB2), YUVG(0xB3),
  YUVG(0xB4), YUVG(0xB5), YUVG(0xB6), YUVG(0xB7),
  YUVG(0xB8), YUVG(0xB9), YUVG(0xBA), YUVG(0xBB),
  YUVG(0xBC), YUVG(0xBD), YUVG(0xBE), YUVG(0xBF),
  YUVG(0xC0), YUVG(0xC1), YUVG(0xC2), YUVG(0xC3),
  YUVG(0xC4), YUVG(0xC5), YUVG(0xC6), YUVG(0xC7),
  YUVG(0xC8), YUVG(0xC9), YUVG(0xCA), YUVG(0xCB),
  YUVG(0xCC), YUVG(0xCD), YUVG(0xCE), YUVG(0xCF),
  YUVG(0xD0), YUVG(0xD1), YUVG(0xD2), YUVG(0xD3),
  YUVG(0xD4), YUVG(0xD5), YUVG(0xD6), YUVG(0xD7),
  YUVG(0xD8), YUVG(0xD9), YUVG(0xDA), YUVG(0xDB),
  YUVG(0xDC), YUVG(0xDD), YUVG(0xDE), YUVG(0xDF),
  YUVG(0xE0), YUVG(0xE1), YUVG(0xE2), YUVG(0xE3),
  YUVG(0xE4), YUVG(0xE5), YUVG(0xE6), YUVG(0xE7),
  YUVG(0xE8), YUVG(0xE9), YUVG(0xEA), YUVG(0xEB),
  YUVG(0xEC), YUVG(0xED), YUVG(0xEE), YUVG(0xEF),
  YUVG(0xF0), YUVG(0xF1), YUVG(0xF2), YUVG(0xF3),
  YUVG(0xF4), YUVG(0xF5), YUVG(0xF6), YUVG(0xF7),
  YUVG(0xF8), YUVG(0xF9), YUVG(0xFA), YUVG(0xFB),
  YUVG(0xFC), YUVG(0xFD), YUVG(0xFE), YUVG(0xFF),

  // B table.
  YUVB(0x00), YUVB(0x01), YUVB(0x02), YUVB(0x03),
  YUVB(0x04), YUVB(0x05), YUVB(0x06), YUVB(0x07),
  YUVB(0x08), YUVB(0x09), YUVB(0x0A), YUVB(0x0B),
  YUVB(0x0C), YUVB(0x0D), YUVB(0x0E), YUVB(0x0F),
  YUVB(0x10), YUVB(0x11), YUVB(0x12), YUVB(0x13),
  YUVB(0x14), YUVB(0x15), YUVB(0x16), YUVB(0x17),
  YUVB(0x18), YUVB(0x19), YUVB(0x1A), YUVB(0x1B),
  YUVB(0x1C), YUVB(0x1D), YUVB(0x1E), YUVB(0x1F),
  YUVB(0x20), YUVB(0x21), YUVB(0x22), YUVB(0x23),
  YUVB(0x24), YUVB(0x25), YUVB(0x26), YUVB(0x27),
  YUVB(0x28), YUVB(0x29), YUVB(0x2A), YUVB(0x2B),
  YUVB(0x2C), YUVB(0x2D), YUVB(0x2E), YUVB(0x2F),
  YUVB(0x30), YUVB(0x31), YUVB(0x32), YUVB(0x33),
  YUVB(0x34), YUVB(0x35), YUVB(0x36), YUVB(0x37),
  YUVB(0x38), YUVB(0x39), YUVB(0x3A), YUVB(0x3B),
  YUVB(0x3C), YUVB(0x3D), YUVB(0x3E), YUVB(0x3F),
  YUVB(0x40), YUVB(0x41), YUVB(0x42), YUVB(0x43),
  YUVB(0x44), YUVB(0x45), YUVB(0x46), YUVB(0x47),
  YUVB(0x48), YUVB(0x49), YUVB(0x4A), YUVB(0x4B),
  YUVB(0x4C), YUVB(0x4D), YUVB(0x4E), YUVB(0x4F),
  YUVB(0x50), YUVB(0x51), YUVB(0x52), YUVB(0x53),
  YUVB(0x54), YUVB(0x55), YUVB(0x56), YUVB(0x57),
  YUVB(0x58), YUVB(0x59), YUVB(0x5A), YUVB(0x5B),
  YUVB(0x5C), YUVB(0x5D), YUVB(0x5E), YUVB(0x5F),
  YUVB(0x60), YUVB(0x61), YUVB(0x62), YUVB(0x63),
  YUVB(0x64), YUVB(0x65), YUVB(0x66), YUVB(0x67),
  YUVB(0x68), YUVB(0x69), YUVB(0x6A), YUVB(0x6B),
  YUVB(0x6C), YUVB(0x6D), YUVB(0x6E), YUVB(0x6F),
  YUVB(0x70), YUVB(0x71), YUVB(0x72), YUVB(0x73),
  YUVB(0x74), YUVB(0x75), YUVB(0x76), YUVB(0x77),
  YUVB(0x78), YUVB(0x79), YUVB(0x7A), YUVB(0x7B),
  YUVB(0x7C), YUVB(0x7D), YUVB(0x7E), YUVB(0x7F),
  YUVB(0x80), YUVB(0x81), YUVB(0x82), YUVB(0x83),
  YUVB(0x84), YUVB(0x85), YUVB(0x86), YUVB(0x87),
  YUVB(0x88), YUVB(0x89), YUVB(0x8A), YUVB(0x8B),
  YUVB(0x8C), YUVB(0x8D), YUVB(0x8E), YUVB(0x8F),
  YUVB(0x90), YUVB(0x91), YUVB(0x92), YUVB(0x93),
  YUVB(0x94), YUVB(0x95), YUVB(0x96), YUVB(0x97),
  YUVB(0x98), YUVB(0x99), YUVB(0x9A), YUVB(0x9B),
  YUVB(0x9C), YUVB(0x9D), YUVB(0x9E), YUVB(0x9F),
  YUVB(0xA0), YUVB(0xA1), YUVB(0xA2), YUVB(0xA3),
  YUVB(0xA4), YUVB(0xA5), YUVB(0xA6), YUVB(0xA7),
  YUVB(0xA8), YUVB(0xA9), YUVB(0xAA), YUVB(0xAB),
  YUVB(0xAC), YUVB(0xAD), YUVB(0xAE), YUVB(0xAF),
  YUVB(0xB0), YUVB(0xB1), YUVB(0xB2), YUVB(0xB3),
  YUVB(0xB4), YUVB(0xB5), YUVB(0xB6), YUVB(0xB7),
  YUVB(0xB8), YUVB(0xB9), YUVB(0xBA), YUVB(0xBB),
  YUVB(0xBC), YUVB(0xBD), YUVB(0xBE), YUVB(0xBF),
  YUVB(0xC0), YUVB(0xC1), YUVB(0xC2), YUVB(0xC3),
  YUVB(0xC4), YUVB(0xC5), YUVB(0xC6), YUVB(0xC7),
  YUVB(0xC8), YUVB(0xC9), YUVB(0xCA), YUVB(0xCB),
  YUVB(0xCC), YUVB(0xCD), YUVB(0xCE), YUVB(0xCF),
  YUVB(0xD0), YUVB(0xD1), YUVB(0xD2), YUVB(0xD3),
  YUVB(0xD4), YUVB(0xD5), YUVB(0xD6), YUVB(0xD7),
  YUVB(0xD8), YUVB(0xD9), YUVB(0xDA), YUVB(0xDB),
  YUVB(0xDC), YUVB(0xDD), YUVB(0xDE), YUVB(0xDF),
  YUVB(0xE0), YUVB(0xE1), YUVB(0xE2), YUVB(0xE3),
  YUVB(0xE4), YUVB(0xE5), YUVB(0xE6), YUVB(0xE7),
  YUVB(0xE8), YUVB(0xE9), YUVB(0xEA), YUVB(0xEB),
  YUVB(0xEC), YUVB(0xED), YUVB(0xEE), YUVB(0xEF),
  YUVB(0xF0), YUVB(0xF1), YUVB(0xF2), YUVB(0xF3),
  YUVB(0xF4), YUVB(0xF5), YUVB(0xF6), YUVB(0xF7),
  YUVB(0xF8), YUVB(0xF9), YUVB(0xFA), YUVB(0xFB),
  YUVB(0xFC), YUVB(0xFD), YUVB(0xFE), YUVB(0xFF),
};

#undef RGBY
#undef RGBU
#undef RGBV
#undef YUVR
#undef YUVG
#undef YUVB

}  // extern "C"
