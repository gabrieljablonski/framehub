#include <iostream>
#include <execinfo.h>
#include <signal.h>
#include <inttypes.h>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}

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

#define MAX_STREAMS 10
AVFormatContext *producer_context;
bool producer_connected = false;
uint64_t dts_offsets[MAX_STREAMS] = { 0 };
uint64_t last_dtss[MAX_STREAMS] = { 0 };

AVPacket *pkt;
pthread_mutex_t pkts_mutex;

void lock_mutex() {
  pthread_mutex_lock(&pkts_mutex);
}

void unlock_mutex() {
  pthread_mutex_unlock(&pkts_mutex);
}

void *producer_handler(void *ptr) {
  AVPacket *ppkt;
  AVInputFormat *format = av_find_input_format("nut");
  int port = *(int *)ptr;

  while (1) {
    char url[1024];
    sprintf(url, "tcp://0.0.0.0:%d?listen", port);
    std::cerr << "waiting for producer on " << url << std::endl;
    int ret;
    producer_context = avformat_alloc_context();
    ret = avformat_open_input(&producer_context, url, format, NULL);

    std::cerr << "producer connected\n";

    if (ret < 0) {
      std::cerr << "failed to open input " << ret << std::endl;
      goto producer_exit;
    }

    ret = avformat_find_stream_info(producer_context, 0);
    if (ret < 0) {
      std::cerr << "`avformat_find_stream_info()` failed " << ret << std::endl;
      goto producer_exit;
    }

    av_dump_format(producer_context, 0, url, 0);

    producer_connected = true;

    ppkt = av_packet_alloc();
    av_init_packet(ppkt);
    while (1) {
      ret = av_read_frame(producer_context, ppkt);
      if (ret < 0) {
        pkt = NULL;
        std::cerr << "failed to read frame " << ret << std::endl;
        break;
      }
      ppkt->dts += dts_offsets[ppkt->stream_index];
      ppkt->pts = ppkt->dts;
      last_dtss[ppkt->stream_index] = ppkt->dts;
      lock_mutex();
      av_packet_free(&pkt);
      pkt = av_packet_clone(ppkt);
      unlock_mutex();
    }
producer_exit:
    memcpy(dts_offsets, last_dtss, MAX_STREAMS*sizeof(uint64_t));
    producer_connected = false;
    av_packet_free(&ppkt);
    avformat_close_input(&producer_context);
    avformat_free_context(producer_context);
    producer_context = NULL;
    std::cerr << "producer dropped\n";
  }
  return 0;
}

void *consumer_handler(void *ptr) {
  AVFormatContext *consumer_context;
  AVOutputFormat* format = av_guess_format("nut", NULL, NULL);
  int port = *(int *)ptr;
  char url[1024];
  sprintf(url, "tcp://0.0.0.0:%d?listen", port);
  int64_t last_dts = 0;

  std::cerr << "setting up output to " << url << std::endl;
  int ret = avformat_alloc_output_context2(&consumer_context, format, NULL, url);
  if (ret < 0) {
    std::cerr << "`avformat_alloc_output_context2()` failed " << ret << std::endl;
    return 0;
  }

  while (!producer_connected || !producer_context);

  for (int i = 0; i < producer_context->nb_streams; i++) {
    AVStream *out_stream;
    AVStream *in_stream = producer_context->streams[i];
    AVCodecParameters *in_codecpar = in_stream->codecpar;

    if (in_codecpar->codec_type != AVMEDIA_TYPE_AUDIO &&
      in_codecpar->codec_type != AVMEDIA_TYPE_VIDEO &&
      in_codecpar->codec_type != AVMEDIA_TYPE_SUBTITLE) {
      continue;
    }

    out_stream = avformat_new_stream(consumer_context, NULL);
    if (!out_stream) {
      std::cerr << "Failed allocating output stream\n";
      ret = AVERROR_UNKNOWN;
      goto consumer_fail;
    }

    ret = avcodec_parameters_copy(out_stream->codecpar, in_codecpar);
    if (ret < 0) {
      std::cerr << "Failed to copy codec parameters\n";
      goto consumer_fail;
    }
  }
  
  av_dump_format(consumer_context, 0, url, 1);

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
  while (1) {
    lock_mutex();
    if (!pkt) { 
      unlock_mutex();
      continue;
    }
    if (pkt->dts == last_dts) {
      unlock_mutex();
      continue;
    }
    ret = av_write_frame(consumer_context, pkt);
    last_dts = pkt->dts;
    unlock_mutex();
    if (ret < 0) {
      std::cerr << "`av_write_frame()` failed " << ret << std::endl;
      if (ret != -22)
        goto consumer_fail;
    }
  }
consumer_fail:
  avformat_free_context(consumer_context);
  std::cerr << "consumer dropped\n";
  return 0;
}

int main(int argc, char **argv) {
  pthread_t producer_thread, consumer_thread;
  int producer_port = 5000,
      consumers_port = 5001;

  if (argc > 1) {
    producer_port = atoi(argv[1]);
  }
  if (argc > 2) {
    consumers_port = atoi(argv[2]);
  }

  signal(SIGSEGV, print_trace);
  
  pthread_mutex_init(&pkts_mutex, NULL);
  pthread_create(&producer_thread, NULL, producer_handler, &producer_port);
  pthread_create(&consumer_thread, NULL, consumer_handler, &consumers_port);
  pthread_join(producer_thread, NULL);
  pthread_join(consumer_thread, NULL);
  pthread_mutex_destroy(&pkts_mutex);
  return 0;
}
