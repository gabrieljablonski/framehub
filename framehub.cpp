#include <iostream>
#include <execinfo.h>
#include <signal.h>
#include <inttypes.h>
#include <unistd.h>
#include <algorithm>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}

#include "frame_queue.h"

void print_trace(int nSig)
{
  printf("print_trace: got signal %d\n", nSig);

  void           *array[32];    /* Array to store backtrace symbols */
  size_t          size;     /* To store the exact no of values stored */
  char          **strings;    /* To store functions from the backtrace list in ARRAY */
  size_t          nCnt;

  size = backtrace(array, 32);

  strings = backtrace_symbols(array, size);

  /* prints each string of function names of trace*/
  for (nCnt = 0; nCnt < size; nCnt++)
    fprintf(stderr, "%s\n", strings[nCnt]);

  exit(-1);
}

void lock_mutex(pthread_mutex_t *mutex) {
  pthread_mutex_lock(mutex);
}

void unlock_mutex(pthread_mutex_t *mutex) {
  pthread_mutex_unlock(mutex);
}

namespace framehub {

const int kMaxErrors = 10;
const int kMaxStreams = 2;
AVFormatContext *producer_context;
int64_t dts_offsets[kMaxStreams] = { 0 };
int64_t last_dtss[kMaxStreams] = { 0 };

FrameQueue video_frames(30);
FrameQueue audio_frames(42);
pthread_mutex_t producer_connected;
uint8_t consumers_connected = 0;

int open_input_codec(AVFormatContext *avctx, AVCodecContext **ctx, AVMediaType type) {
  AVCodec *codec;
  int ret = av_find_best_stream(avctx, type, -1, -1, &codec, 0);
  int stream = ret;
  if (ret < 0) {
    std::cerr << "`av_find_best_stream()` failed " << ret << std::endl;
    return ret;
  }

  if (!(*ctx = avcodec_alloc_context3(codec))) {
    std::cerr << "`avcodec_alloc_context3()` failed\n";
    return -1;
  }

  ret = avcodec_parameters_to_context(*ctx, avctx->streams[stream]->codecpar);
  if (ret < 0) {
    std::cerr << "`avcodec_parameters_to_context()` failed " << ret << std::endl;
    return ret;
  }

  ret = avcodec_open2(*ctx, codec, NULL);
  if (ret < 0) {
    std::cerr << "`avcodec_open2()` failed " << ret << std::endl;
    return ret;
  }

  return stream;
}

int open_output_codec(AVFormatContext *in_ctx, AVFormatContext *out_ctx, AVCodecContext **ctx, AVMediaType type) {
  AVCodec *codec;
  AVStream *in_stream;
  AVStream *out_stream;
  int ret = av_find_best_stream(in_ctx, type, -1, -1, NULL, 0);
  if (ret < 0) {
    std::cerr << "`av_find_best_stream()` failed " << ret << std::endl;
    return ret;
  }
  int stream = ret;

  in_stream = in_ctx->streams[stream];
  codec = avcodec_find_encoder(in_stream->codecpar->codec_id);
  if (!codec) {
    std::cerr << "`avcodec_find_encoder()` failed " << ret << std::endl;
    return ret;
  }

  if (!(*ctx = avcodec_alloc_context3(codec))) {
    std::cerr << "`avcodec_alloc_context3()` failed\n";
    return -1;
  }

  out_stream = avformat_new_stream(out_ctx, NULL);
  if (!out_stream) {
    std::cerr << "Failed allocating output stream\n";
    return -1;
  }

  ret = avcodec_parameters_copy(out_stream->codecpar, in_stream->codecpar);
  if (ret < 0) {
    std::cerr << "Failed to copy codec parameters\n";
    return ret;
  }
  
  out_stream->codecpar->codec_tag = in_stream->codecpar->codec_tag;
  out_stream->time_base = in_stream->time_base;
  out_stream->avg_frame_rate = in_stream->avg_frame_rate;
  out_stream->r_frame_rate = in_stream->r_frame_rate;
  
  ret = avcodec_parameters_to_context(*ctx, out_stream->codecpar);
  if (ret < 0) {
    std::cerr << "`avcodec_parameters_to_context()` failed " << ret << std::endl;
    return ret;
  }

  (*ctx)->time_base = out_stream->time_base;

  ret = avcodec_open2(*ctx, codec, NULL);
  if (ret < 0) {
    std::cerr << "`avcodec_open2()` failed " << ret << std::endl;
    return ret;
  }

  return stream;
}

int send_pkt_receive_frame(AVCodecContext *ctx, AVPacket *pkt, AVFrame *frame) {
  int ret = avcodec_send_packet(ctx, pkt);
  if (ret < 0) {
    std::cerr << "`avcodec_send_packet()` failed " << ret << std::endl;
    return ret;
  }

  ret = avcodec_receive_frame(ctx, frame);
  if (ret < 0) {
    std::cerr << "`avcodec_receive_frame()` failed " << ret << std::endl;
    return ret;
  }

  return 0;
}

void *producer_handler(void *ptr) {
  int port = *(int *)ptr;
  char url[1024];
  int error_count = 0;
  int64_t v_dts = 0;
  int64_t a_dts = 0;
  int64_t frame_count = 0;
  int64_t vfcount = 0;
  int64_t afcount = 0;
  int video_stream = -1;
  int audio_stream = -1;

  std::chrono::steady_clock::time_point video_live_until;
  std::chrono::steady_clock::time_point audio_live_until;
  auto start = std::chrono::steady_clock::now();

  AVInputFormat *format = av_find_input_format("nut");
  AVCodecContext *video_codec_context;
  AVCodecContext *audio_codec_context;

  sprintf(url, "tcp://0.0.0.0:%d?listen", port);

  while (1) {
    std::cerr << "waiting for producer on " << url << std::endl;
    int ret;
    producer_context = avformat_alloc_context();
    ret = avformat_open_input(&producer_context, url, format, NULL);

    std::cerr << "producer connected\n";

    if (ret < 0) {
      std::cerr << "failed to open input " << ret << std::endl;
      goto producer_fail;
    }

    ret = avformat_find_stream_info(producer_context, 0);
    if (ret < 0) {
      std::cerr << "`avformat_find_stream_info()` failed " << ret << std::endl;
      goto producer_fail;
    }

    av_dump_format(producer_context, 0, url, 0);
    std::cerr << "\n\n";

    if ((video_stream = open_input_codec(producer_context, &video_codec_context, AVMEDIA_TYPE_VIDEO)) < 0) {
      goto producer_fail;
    }

    if ((audio_stream = open_input_codec(producer_context, &audio_codec_context, AVMEDIA_TYPE_AUDIO)) < 0) {
      goto producer_fail;
    }

    unlock_mutex(&producer_connected);
    frame_count = 0;
    vfcount = 0;
    afcount = 0;
    video_live_until = std::chrono::steady_clock::now();
    audio_live_until = std::chrono::steady_clock::now();
    while (1) {
      AVPacket ppkt;
      AVCodecContext *pctx;
      AVFrame *pframe = av_frame_alloc();
      ret = av_read_frame(producer_context, &ppkt);
      if (ret == AVERROR_EOF) {
        std::cerr << "producer EOF" << std::endl;
        goto producer_fail;
      }
      if (ret < 0) {
        ++error_count;
        std::cerr << "failed to read frame " << ret << std::endl;
        if (error_count > kMaxErrors)
          goto producer_fail;
        goto read_frame_end;
      }

      error_count = 0;
      ppkt.dts += dts_offsets[ppkt.stream_index];
      ppkt.pts = ppkt.dts;
      last_dtss[ppkt.stream_index] = ppkt.dts + ppkt.duration;

      pctx = ppkt.stream_index == video_stream ? video_codec_context : audio_codec_context;

      if ((ret = send_pkt_receive_frame(pctx, &ppkt, pframe)) < 0) {
        goto producer_fail;
      }
      if (ppkt.stream_index == video_stream) {
        AVRational fps = producer_context->streams[video_stream]->r_frame_rate;
        video_live_until += std::chrono::microseconds((1000000*fps.den)/fps.num);

        video_frames.PushBack(new Frame(pframe, ppkt.dts, ppkt.stream_index, frame_count++, video_live_until));

        if (ppkt.dts && v_dts && ppkt.dts != v_dts + ppkt.duration)
          std::cerr << "p:" << frame_count - 1 << " " << ppkt.dts << " **** dropped video frame\n";
        v_dts = ppkt.dts;
      } else if (ppkt.stream_index == audio_stream) {
        audio_live_until += std::chrono::microseconds((1000000 / (pframe->sample_rate/pframe->nb_samples)));
        audio_frames.PushBack(new Frame(pframe, ppkt.dts, ppkt.stream_index, frame_count++, audio_live_until));
        // if (ppkt.dts && ppkt.dts != a_dts + ppkt.duration)
        //   std::cerr << "p:" << frame_count - 1 << " " << ppkt.dts << " != " << a_dts << " + " << ppkt.duration << " **** dropped audio frame\n";
        a_dts = ppkt.dts;
      }
      // std::cerr << frame_count << ":" << ppkt.stream_index << std::endl;
read_frame_end:
      av_packet_unref(&ppkt);
    }
producer_fail:
    memcpy(dts_offsets, last_dtss, kMaxStreams*sizeof(uint64_t));
    lock_mutex(&producer_connected);
    avformat_close_input(&producer_context);
    avformat_free_context(producer_context);
    avcodec_free_context(&video_codec_context);
    producer_context = NULL;
    std::cerr << "producer dropped\n";
  }
  return 0;
}

static void *consumer_handler(void *ptr) {
  int port = *(int *)ptr;
  char url[1024];
  int error_count = 0;
  int64_t expected_frame = -1;
  uint8_t consumer_id = consumers_connected++;
  const size_t kMaxReceived = 50;
  int64_t received[kMaxReceived];
  size_t received_n = 0;
  
  AVFormatContext *consumer_context;
  AVOutputFormat* format = av_guess_format("nut", NULL, NULL);
  AVCodecContext *video_codec_context;
  AVCodecContext *audio_codec_context;

  sprintf(url, "tcp://0.0.0.0:%d?listen", port);

  std::cerr << "setting up output to " << url << std::endl;
  int ret = avformat_alloc_output_context2(&consumer_context, format, NULL, url);
  if (ret < 0) {
    std::cerr << "`avformat_alloc_output_context2()` failed " << ret << std::endl;
    return 0;
  }

  lock_mutex(&producer_connected);

  if (open_output_codec(producer_context, consumer_context, &video_codec_context, AVMEDIA_TYPE_VIDEO) < 0) {
    goto consumer_fail;
  }
  if (open_output_codec(producer_context, consumer_context, &audio_codec_context, AVMEDIA_TYPE_AUDIO) < 0) {
    goto consumer_fail;
  }
  
  unlock_mutex(&producer_connected);
  
  // av_dump_format(consumer_context, 0, url, 1);
  // std::cerr << "\n\n";

  if (!(consumer_context->oformat->flags & AVFMT_NOFILE)) {
    avio_open(&consumer_context->pb, url, AVIO_FLAG_WRITE);
  }

  pthread_t next_consumer;
  pthread_create(&next_consumer, NULL, consumer_handler, ptr);

  ret = avformat_init_output(consumer_context, NULL);
  if (ret < 0) {
    std::cerr << "`avformat_init_output()` failed " << ret << std::endl;
    goto consumer_fail;
  }

  if (ret == AVSTREAM_INIT_IN_WRITE_HEADER) {
    ret = avformat_write_header(consumer_context, NULL);
    if (ret < 0) {
      std::cerr << "`avformat_write_header()` failed " << ret << std::endl;
      goto consumer_fail;
    }
  }

  for (int i = 0; i < kMaxReceived; ++i) {
    received[i] = -1;
  }

  while (1) {
    Frame *videof = video_frames.PeekNext(consumer_id);
    Frame *audiof = audio_frames.PeekNext(consumer_id);
    if (!videof && !audiof) {
      usleep(500); // TODO: wait for PopFront() signal
      continue;
    }

    bool is_video;
    if (!videof) {
      is_video = false;
    } else if (!audiof) {
      is_video = true;
    } else {
      is_video = videof->GetNumber() < audiof->GetNumber();
    }

    Frame *frame = (is_video ? videof : audiof)->Clone(consumer_id);

    if (expected_frame == -1)
      expected_frame = frame->GetNumber();

    // std::cerr << frame->GetNumber() << ":" << expected_frame << std::endl;

    while (frame->GetNumber() != expected_frame) {
      bool found = false;
      for (int i = 0; i < kMaxReceived; ++i) {
        if (received[i] == expected_frame) {
          received[i] = -1;
          found = true;
          --received_n;
          break;
        }
      }
      if (found) {
        ++expected_frame;
        continue;
      }

      for (int i = 0; i < kMaxReceived; ++i) {
        if (received[i] == -1) {
          received[i] = frame->GetNumber();
          ++received_n;
          break;
        }
      }
      if (received_n == kMaxReceived) {
        std::cerr << "c(" << int{consumer_id} << "):" << expected_frame << " **** dropped\n";
        ++expected_frame;
        continue;
      }
      break;
    }

    if (frame->GetNumber() == expected_frame)
      ++expected_frame;

    AVCodecContext *ctx = is_video ? video_codec_context : audio_codec_context;
    AVPacket cpkt;

    ret = avcodec_send_frame(ctx, frame->GetFrame());
    if (ret < 0) {
      std::cerr << "`avcodec_send_frame()` failed " << ret << std::endl;
      if (ret == AVERROR_EOF)
        ret = 0;
      goto write_frame_end;
    }

    ret = avcodec_receive_packet(ctx, &cpkt);
    if (ret < 0) {
      std::cerr << "`avcodec_receive_packet()` failed " << ret << std::endl;
      if (ret == AVERROR_EOF)
        ret = 0;
      goto write_frame_end;
    }

    cpkt.dts = frame->GetDts();
    cpkt.stream_index = frame->GetStream();
    ret = av_write_frame(consumer_context, &cpkt);
    if (ret < 0) {
      std::cerr << "`av_write_frame()` failed " << ret << std::endl;
      ++error_count;
      if (error_count <= kMaxErrors)
        ret = 0;
      goto write_frame_end;
    }
    error_count = 0;
write_frame_end:
    av_packet_unref(&cpkt);
    frame->Free();
    if (ret)
      goto consumer_fail;
  }
consumer_fail:
  // avcodec_flush_buffers(video_codec_context);
  // avcodec_flush_buffers(audio_codec_context);
  avformat_free_context(consumer_context);
  std::cerr << "consumer dropped\n";
  return 0;
}

void *dispose_frames(void *ptr) {
  while (1) {
    usleep(14000);
    video_frames.TryPopFront();
    audio_frames.TryPopFront();
  }
}

int main(int argc, char **argv) {
  pthread_t producer_thread, consumer_thread, dispose_thread;
  int producer_port = 5000,
      consumers_port = 5001;

  if (argc > 1) {
    producer_port = atoi(argv[1]);
  }
  if (argc > 2) {
    consumers_port = atoi(argv[2]);
  }

  signal(SIGSEGV, print_trace);
  
  pthread_mutex_init(&producer_connected, NULL);
  lock_mutex(&producer_connected);
  pthread_create(&producer_thread, NULL, producer_handler, &producer_port);
  pthread_create(&consumer_thread, NULL, consumer_handler, &consumers_port);
  pthread_create(&dispose_thread, NULL, dispose_frames, NULL);
  pthread_join(producer_thread, NULL);
  pthread_join(consumer_thread, NULL);
  pthread_join(dispose_thread, NULL);
  pthread_mutex_destroy(&producer_connected);
  return 0;
}

} // namespace framehub

int main(int argc, char **argv) {
  framehub::main(argc, argv);
}
