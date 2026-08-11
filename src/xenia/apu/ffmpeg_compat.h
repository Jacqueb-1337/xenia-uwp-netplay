// Compatibility helpers for the FFmpeg versions used by Xenia's desktop and
// legacy UWP build paths.

#ifndef XENIA_APU_FFMPEG_COMPAT_H_
#define XENIA_APU_FFMPEG_COMPAT_H_

extern "C" {
#include "third_party/FFmpeg/libavcodec/avcodec.h"
#include "third_party/FFmpeg/libavutil/channel_layout.h"
#include "third_party/FFmpeg/libavutil/frame.h"
}

namespace xe::apu {

inline int GetAvChannelCount(const AVCodecContext* context) {
#if LIBAVCODEC_VERSION_MAJOR >= 59
  return context->ch_layout.nb_channels;
#else
  return context->channels;
#endif
}

inline int GetAvChannelCount(const AVFrame* frame) {
#if LIBAVCODEC_VERSION_MAJOR >= 59
  return frame->ch_layout.nb_channels;
#else
  return frame->channels;
#endif
}

inline void ResetAvChannelLayout(AVCodecContext* context) {
#if LIBAVCODEC_VERSION_MAJOR >= 59
  context->ch_layout = AVChannelLayout{};
#else
  context->channels = 0;
  context->channel_layout = 0;
#endif
}

inline void SetAvChannelLayout(AVCodecContext* context, int channels) {
#if LIBAVCODEC_VERSION_MAJOR >= 59
  av_channel_layout_default(&context->ch_layout, channels);
#else
  context->channels = channels;
  context->channel_layout = av_get_default_channel_layout(channels);
#endif
}

}  // namespace xe::apu

#endif  // XENIA_APU_FFMPEG_COMPAT_H_
