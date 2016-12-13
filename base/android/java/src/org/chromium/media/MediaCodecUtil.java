// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.media;

import android.annotation.TargetApi;
import android.media.MediaCodec;
import android.media.MediaCodec.CryptoInfo;
import android.media.MediaCodecInfo;
import android.media.MediaCodecList;
import android.os.Build;

import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.MainDex;

import java.util.Arrays;
import java.util.List;
import java.util.Locale;

/**
 * A collection of MediaCodec utility functions.
 */
@JNINamespace("media")
@MainDex
class MediaCodecUtil {
    private static final String TAG = "cr_MediaCodecUtil";

    // Codec direction.  Keep this in sync with media_codec_direction.h.
    static final int MEDIA_CODEC_DECODER = 0;
    static final int MEDIA_CODEC_ENCODER = 1;

    /**
     * Class to pass parameters from createDecoder()
     */
    public static class CodecCreationInfo {
        public MediaCodec mediaCodec;
        public boolean supportsAdaptivePlayback;
        public BitrateAdjustmentTypes bitrateAdjustmentType = BitrateAdjustmentTypes.NO_ADJUSTMENT;
    }

    public static final class MimeTypes {
        public static final String VIDEO_MP4 = "video/mp4";
        public static final String VIDEO_WEBM = "video/webm";
        public static final String VIDEO_H264 = "video/avc";
        public static final String VIDEO_H265 = "video/hevc";
        public static final String VIDEO_VP8 = "video/x-vnd.on2.vp8";
        public static final String VIDEO_VP9 = "video/x-vnd.on2.vp9";
    }

    // Type of bitrate adjustment for video encoder.
    public enum BitrateAdjustmentTypes {
        // No adjustment - video encoder has no known bitrate problem.
        NO_ADJUSTMENT,
        // Framerate based bitrate adjustment is required - HW encoder does not use frame
        // timestamps to calculate frame bitrate budget and instead is relying on initial
        // fps configuration assuming that all frames are coming at fixed initial frame rate.
        FRAMERATE_ADJUSTMENT,
    }

    /**
     * Class to abstract platform version API differences for interacting with
     * the MediaCodecList.
     */
    private static class MediaCodecListHelper {
        @TargetApi(Build.VERSION_CODES.LOLLIPOP)
        public MediaCodecListHelper() {
            if (hasNewMediaCodecList()) {
                mCodecList = new MediaCodecList(MediaCodecList.ALL_CODECS).getCodecInfos();
            }
        }

        @SuppressWarnings("deprecation")
        public int getCodecCount() {
            if (hasNewMediaCodecList()) return mCodecList.length;
            return MediaCodecList.getCodecCount();
        }

        @SuppressWarnings("deprecation")
        public MediaCodecInfo getCodecInfoAt(int index) {
            if (hasNewMediaCodecList()) return mCodecList[index];
            return MediaCodecList.getCodecInfoAt(index);
        }

        private boolean hasNewMediaCodecList() {
            return Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP;
        }

        private MediaCodecInfo[] mCodecList;
    }

    /**
     * Return true if and only if name is a software codec.
     * @param name The codec name, e.g. from MediaCodecInfo.getName().
     */
    public static boolean isSoftwareCodec(String name) {
        // This is structured identically to libstagefright/OMXCodec.cpp .
        if (name.startsWith("OMX.google.")) return true;

        if (name.startsWith("OMX.")) return false;

        return true;
    }

    /**
     * Get a name of default android codec.
     * @param mime MIME type of the media.
     * @param direction Whether this is encoder or decoder.
     * @param requireSoftwareCodec Whether we require a software codec.
     * @return name of the codec.
     */
    @CalledByNative
    private static String getDefaultCodecName(
            String mime, int direction, boolean requireSoftwareCodec) {
        MediaCodecListHelper codecListHelper = new MediaCodecListHelper();
        int codecCount = codecListHelper.getCodecCount();
        for (int i = 0; i < codecCount; ++i) {
            MediaCodecInfo info = codecListHelper.getCodecInfoAt(i);

            int codecDirection = info.isEncoder() ? MEDIA_CODEC_ENCODER : MEDIA_CODEC_DECODER;
            if (codecDirection != direction) continue;

            if (requireSoftwareCodec && !isSoftwareCodec(info.getName())) continue;

            String[] supportedTypes = info.getSupportedTypes();
            for (int j = 0; j < supportedTypes.length; ++j) {
                if (supportedTypes[j].equalsIgnoreCase(mime)) return info.getName();
            }
        }

        Log.e(TAG, "Decoder for type %s is not supported on this device", mime);
        return "";
    }

    /**
     * Get a list of encoder supported color formats for specified MIME type.
     * @param mime MIME type of the media format.
     * @return a list of encoder supported color formats.
     */
    @CalledByNative
    private static int[] getEncoderColorFormatsForMime(String mime) {
        MediaCodecListHelper codecListHelper = new MediaCodecListHelper();
        int codecCount = codecListHelper.getCodecCount();
        for (int i = 0; i < codecCount; i++) {
            MediaCodecInfo info = codecListHelper.getCodecInfoAt(i);
            if (!info.isEncoder()) continue;

            String[] supportedTypes = info.getSupportedTypes();
            for (int j = 0; j < supportedTypes.length; ++j) {
                if (supportedTypes[j].equalsIgnoreCase(mime)) {
                    return info.getCapabilitiesForType(supportedTypes[j]).colorFormats;
                }
            }
        }
        return null;
    }

    /**
      * Check if a given MIME type can be decoded.
      * @param mime MIME type of the media.
      * @param secure Whether secure decoder is required.
      * @return true if system is able to decode, or false otherwise.
      */
    @CalledByNative
    private static boolean canDecode(String mime, boolean isSecure) {
        // TODO(liberato): Should we insist on software here?
        CodecCreationInfo info = createDecoder(mime, isSecure, false);
        if (info.mediaCodec == null) return false;

        try {
            info.mediaCodec.release();
        } catch (IllegalStateException e) {
            Log.e(TAG, "Cannot release media codec", e);
        }
        return true;
    }

    /**
     * Creates MediaCodec decoder.
     * @param mime MIME type of the media.
     * @param secure Whether secure decoder is required.
     * @param requireSoftwareCodec Whether a software decoder is required.
     * @return CodecCreationInfo object
     */
    static CodecCreationInfo createDecoder(
            String mime, boolean isSecure, boolean requireSoftwareCodec) {
        // Always return a valid CodecCreationInfo, its |mediaCodec| field will be null
        // if we cannot create the codec.
        CodecCreationInfo result = new CodecCreationInfo();

        assert result.mediaCodec == null;

        // Creation of ".secure" codecs sometimes crash instead of throwing exceptions
        // on pre-JBMR2 devices.
        if (isSecure && Build.VERSION.SDK_INT < Build.VERSION_CODES.JELLY_BEAN_MR2) return result;

        // Do not create codec for blacklisted devices.
        if (!isDecoderSupportedForDevice(mime)) {
            Log.e(TAG, "Decoder for type %s is not supported on this device", mime);
            return result;
        }

        try {
            // |isSecure| only applies to video decoders.
            if (mime.startsWith("video") && isSecure) {
                String decoderName =
                        getDefaultCodecName(mime, MEDIA_CODEC_DECODER, requireSoftwareCodec);
                if (decoderName.equals("")) return null;
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.KITKAT) {
                    // To work around an issue that we cannot get the codec info from the secure
                    // decoder, create an insecure decoder first so that we can query its codec
                    // info. http://b/15587335.
                    // Futhermore, it is impossible to create an insecure decoder if the secure
                    // one is already created.
                    MediaCodec insecureCodec = MediaCodec.createByCodecName(decoderName);
                    result.supportsAdaptivePlayback =
                            codecSupportsAdaptivePlayback(insecureCodec, mime);
                    insecureCodec.release();
                }
                result.mediaCodec = MediaCodec.createByCodecName(decoderName + ".secure");
            } else {
                if (requireSoftwareCodec) {
                    String decoderName =
                            getDefaultCodecName(mime, MEDIA_CODEC_DECODER, requireSoftwareCodec);
                    result.mediaCodec = MediaCodec.createByCodecName(decoderName);
                } else {
                    result.mediaCodec = MediaCodec.createDecoderByType(mime);
                }
                result.supportsAdaptivePlayback =
                        codecSupportsAdaptivePlayback(result.mediaCodec, mime);
            }
        } catch (Exception e) {
            Log.e(TAG, "Failed to create MediaCodec: %s, isSecure: %s, requireSoftwareCodec: %s",
                    mime, isSecure, requireSoftwareCodec ? "yes" : "no", e);
            result.mediaCodec = null;
        }
        return result;
    }

    /**
     * This is a way to blacklist misbehaving devices.
     * Some devices cannot decode certain codecs, while other codecs work fine.
     * @param mime MIME type as passed to mediaCodec.createDecoderByType(mime).
     * @return true if this codec is supported for decoder on this device.
     */
    @CalledByNative
    static boolean isDecoderSupportedForDevice(String mime) {
        // *************************************************************
        // *** DO NOT ADD ANY NEW CODECS WITHOUT UPDATING MIME_UTIL. ***
        // *************************************************************
        if (mime.equals("video/x-vnd.on2.vp8")) {
            // Only support VP8 on Android versions where we don't have to synchronously
            // tear down the MediaCodec on surface destruction because VP8 requires us to
            // completely drain the decoder before releasing it, which is difficult and
            // time consuming to do while the surface is being destroyed.
            if (Build.VERSION.SDK_INT < Build.VERSION_CODES.JELLY_BEAN_MR2) return false;

            if (Build.MANUFACTURER.toLowerCase(Locale.getDefault()).equals("samsung")) {
                // Some Samsung devices cannot render VP8 video directly to the surface.

                // Samsung Galaxy S4.
                // Only GT-I9505G with Android 4.3 and SPH-L720 (Sprint) with Android 5.0.1
                // were tested. Only the first device has the problem.
                // We blacklist popular Samsung Galaxy S4 models before Android L.
                if (Build.VERSION.SDK_INT < Build.VERSION_CODES.LOLLIPOP
                        && (Build.MODEL.startsWith("GT-I9505")
                                   || Build.MODEL.startsWith("GT-I9500"))) {
                    return false;
                }

                // Samsung Galaxy S4 Mini.
                // Only GT-I9190 was tested with Android 4.4.2
                // We blacklist it and the popular GT-I9195 for all Android versions.
                if (Build.MODEL.startsWith("GT-I9190") || Build.MODEL.startsWith("GT-I9195")) {
                    return false;
                }

                // Some Samsung devices have problems with WebRTC.
                // We copy blacklisting patterns from software_renderin_list_json.cc
                // although they are broader than the bugs they refer to.

                if (Build.VERSION.SDK_INT < Build.VERSION_CODES.KITKAT) {
                    // Samsung Galaxy Note 2, http://crbug.com/308721.
                    if (Build.MODEL.startsWith("GT-")) return false;

                    // Samsung Galaxy S4, http://crbug.com/329072.
                    if (Build.MODEL.startsWith("SCH-")) return false;

                    // Samsung Galaxy Tab, http://crbug.com/408353.
                    if (Build.MODEL.startsWith("SM-T")) return false;
                }
            }

            // MediaTek decoders do not work properly on vp8. See http://crbug.com/446974 and
            // http://crbug.com/597836.
            if (Build.HARDWARE.startsWith("mt")) return false;
        } else if (mime.equals("video/x-vnd.on2.vp9")) {
            if (Build.VERSION.SDK_INT < Build.VERSION_CODES.KITKAT) return false;

            // MediaTek decoders do not work properly on vp9 before Lollipop. See
            // http://crbug.com/597836.
            if (Build.VERSION.SDK_INT < Build.VERSION_CODES.LOLLIPOP
                    && Build.HARDWARE.startsWith("mt")) {
                return false;
            }
        } else if (mime.equals("audio/opus")
                && Build.VERSION.SDK_INT < Build.VERSION_CODES.LOLLIPOP) {
            return false;
        }
        // *************************************************************
        // *** DO NOT ADD ANY NEW CODECS WITHOUT UPDATING MIME_UTIL. ***
        // *************************************************************
        return true;
    }

    /**
     * Returns true if and only enabling adaptive playback is unsafe.  On some
     * device / os combinations, enabling it causes decoded frames to be
     * unusable.  For example, the S3 on 4.4.2 returns black and white, tiled
     * frames when this is enabled.
     */
    private static boolean isAdaptivePlaybackBlacklisted(String mime) {
        if (!mime.equals("video/avc") && !mime.equals("video/avc1")) {
            return false;
        }

        if (!Build.VERSION.RELEASE.equals("4.4.2")) {
            return false;
        }

        if (!Build.MANUFACTURER.toLowerCase(Locale.getDefault()).equals("samsung")) {
            return false;
        }

        return Build.MODEL.startsWith("GT-I9300") || // S3 (I9300 / I9300I)
                Build.MODEL.startsWith("SCH-I535"); // S3
    }

    /**
     * Returns true if the given codec supports adaptive playback (dynamic resolution change).
     * @param mediaCodec the codec.
     * @param mime MIME type that corresponds to the codec creation.
     * @return true if this codec and mime type combination supports adaptive playback.
     */
    @TargetApi(Build.VERSION_CODES.KITKAT)
    private static boolean codecSupportsAdaptivePlayback(MediaCodec mediaCodec, String mime) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.KITKAT || mediaCodec == null) {
            return false;
        }
        try {
            MediaCodecInfo info = mediaCodec.getCodecInfo();
            if (info.isEncoder()) {
                return false;
            }

            if (isAdaptivePlaybackBlacklisted(mime)) {
                return false;
            }

            MediaCodecInfo.CodecCapabilities capabilities = info.getCapabilitiesForType(mime);
            return (capabilities != null)
                    && capabilities.isFeatureSupported(
                               MediaCodecInfo.CodecCapabilities.FEATURE_AdaptivePlayback);
        } catch (IllegalArgumentException e) {
            Log.e(TAG, "Cannot retrieve codec information", e);
        }
        return false;
    }

    // List of supported HW encoders.
    private static enum HWEncoderProperties {
        QcomVp8(MimeTypes.VIDEO_VP8, "OMX.qcom.", Build.VERSION_CODES.KITKAT,
                BitrateAdjustmentTypes.NO_ADJUSTMENT),
        QcomH264(MimeTypes.VIDEO_H264, "OMX.qcom.", Build.VERSION_CODES.KITKAT,
                BitrateAdjustmentTypes.NO_ADJUSTMENT),
        ExynosVp8(MimeTypes.VIDEO_VP8, "OMX.Exynos.", Build.VERSION_CODES.M,
                BitrateAdjustmentTypes.NO_ADJUSTMENT),
        ExynosH264(MimeTypes.VIDEO_H264, "OMX.Exynos.", Build.VERSION_CODES.LOLLIPOP,
                BitrateAdjustmentTypes.FRAMERATE_ADJUSTMENT);

        private final String mMime;
        private final String mPrefix;
        private final int mMinSDK;
        private final BitrateAdjustmentTypes mBitrateAdjustmentType;

        private HWEncoderProperties(String mime, String prefix, int minSDK,
                BitrateAdjustmentTypes bitrateAdjustmentType) {
            this.mMime = mime;
            this.mPrefix = prefix;
            this.mMinSDK = minSDK;
            this.mBitrateAdjustmentType = bitrateAdjustmentType;
        }

        public String getMime() {
            return mMime;
        }

        public String getPrefix() {
            return mPrefix;
        }

        public int getMinSDK() {
            return mMinSDK;
        }

        public BitrateAdjustmentTypes getBitrateAdjustmentType() {
            return mBitrateAdjustmentType;
        }
    }

    // List of devices with poor H.264 encoder quality.
    private static final String[] H264_ENCODER_MODEL_BLACKLIST = new String[] {
            // HW H.264 encoder on below devices has poor bitrate control - actual bitrates deviates
            // a lot from the target value.
            "SAMSUNG-SGH-I337", "Nexus 7", "Nexus 4"};

    /**
     * Creates MediaCodec encoder.
     * @param mime MIME type of the media.
     * @return CodecCreationInfo object
     */
    static CodecCreationInfo createEncoder(String mime) {
        // Always return a valid CodecCreationInfo, its |mediaCodec| field will be null
        // if we cannot create the codec.
        CodecCreationInfo result = new CodecCreationInfo();

        HWEncoderProperties encoderProperties = findHWEncoder(mime);
        if (encoderProperties == null) {
            return result;
        }

        try {
            result.mediaCodec = MediaCodec.createEncoderByType(mime);
            result.supportsAdaptivePlayback = false;
            result.bitrateAdjustmentType = encoderProperties.getBitrateAdjustmentType();
        } catch (Exception e) {
            Log.e(TAG, "Failed to create MediaCodec: %s", mime, e);
        }
        return result;
    }

    /**
     * This is a way to blacklist misbehaving devices.
     * @param mime MIME type as passed to mediaCodec.createEncoderByType(mime).
     * @return true if this codec is supported for encoder on this device.
     */
    @CalledByNative
    static boolean isEncoderSupportedByDevice(String mime) {
        // MediaCodec.setParameters is missing for JB and below, so bitrate
        // can not be adjusted dynamically.
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.KITKAT) {
            return false;
        }

        // Check if this is supported HW encoder.
        if (mime.equals(MimeTypes.VIDEO_H264)) {
            // Check if device is in H.264 exception list.
            List<String> exceptionModels = Arrays.asList(H264_ENCODER_MODEL_BLACKLIST);
            if (exceptionModels.contains(Build.MODEL)) {
                Log.w(TAG, "Model: " + Build.MODEL + " has blacklisted H.264 encoder.");
                return false;
            }
        }

        if (findHWEncoder(mime) == null) {
            return false;
        }

        return true;
    }

    /**
     * Find HW encoder with given MIME type.
     * @param mime MIME type of the media.
     * @return HWEncoderProperties object.
     */
    private static HWEncoderProperties findHWEncoder(String mime) {
        MediaCodecListHelper codecListHelper = new MediaCodecListHelper();
        int codecCount = codecListHelper.getCodecCount();
        for (int i = 0; i < codecCount; ++i) {
            MediaCodecInfo info = codecListHelper.getCodecInfoAt(i);

            if (!info.isEncoder() || isSoftwareCodec(info.getName())) continue;

            String encoderName = null;
            for (String mimeType : info.getSupportedTypes()) {
                if (mimeType.equalsIgnoreCase(mime)) {
                    encoderName = info.getName();
                    break;
                }
            }

            if (encoderName == null) {
                continue; // No HW support in this codec; try the next one.
            }

            // Check if this is supported HW encoder.
            for (HWEncoderProperties codecProperties : HWEncoderProperties.values()) {
                if (!mime.equalsIgnoreCase(codecProperties.getMime())) continue;

                if (encoderName.startsWith(codecProperties.getPrefix())) {
                    if (Build.VERSION.SDK_INT < codecProperties.getMinSDK()) {
                        Log.w(TAG, "Codec " + encoderName + " is disabled due to SDK version "
                                        + Build.VERSION.SDK_INT);
                        continue;
                    }
                    Log.d(TAG, "Found target encoder for mime " + mime + " : " + encoderName);
                    return codecProperties;
                }
            }
        }

        Log.w(TAG, "HW encoder for " + mime + " is not available on this device.");
        return null;
    }

    /**
     * Returns true if and only if the platform we are running on supports the 'cbcs'
     * encryption scheme, specifically AES CBC encryption with possibility of pattern
     * encryption.
     * While 'cbcs' scheme was originally implemented in N, there was a bug (in the
     * DRM code) which means that it didn't really work properly until post-N).
     */
    static boolean platformSupportsCbcsEncryption() {
        return Build.VERSION.SDK_INT > Build.VERSION_CODES.N;
    }

    /**
     * Sets the encryption pattern value if and only if CryptoInfo.setPattern method is
     * supported.
     * This method was introduced in Android N. Note that if platformSupportsCbcsEncryption
     * returns true, then this function will set the pattern.
     */
    static void setPatternIfSupported(CryptoInfo cryptoInfo, int encrypt, int skip) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
            cryptoInfo.setPattern(new CryptoInfo.Pattern(encrypt, skip));
        }
    }
}
