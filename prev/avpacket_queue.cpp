#include <iostream>

extern "C" {
#include <libavformat/avformat.h>
}

#include "avpacket_queue.h"

void avpacket_queue_init(AVPacketQueue *q, size_t max_packets)
{
  memset(q, 0, sizeof(AVPacketQueue));
  pthread_mutex_init(&q->mutex, NULL);
  pthread_cond_init(&q->cond, NULL);
  q->max_packets = max_packets;
}

void avpacket_queue_flush(AVPacketQueue *q)
{
  AVPacketList *pkt, *pkt1;

  pthread_mutex_lock(&q->mutex);
  for (pkt = q->first_pkt; pkt != NULL; pkt = pkt1) {
    pkt1 = pkt->next;
    AVPacket *p = &pkt->pkt;
    av_packet_free(&p);
    av_freep(&pkt);
  }
  q->last_pkt   = NULL;
  q->first_pkt  = NULL;
  q->nb_packets = 0;
  q->size       = 0;
  pthread_mutex_unlock(&q->mutex);
}

void avpacket_queue_end(AVPacketQueue *q)
{
  avpacket_queue_flush(q);
  pthread_mutex_destroy(&q->mutex);
  pthread_cond_destroy(&q->cond);
}

int avpacket_queue_put(AVPacketQueue *q, const AVPacket *pkt_src)
{
  AVPacketList *pkt1;
  AVPacket *pkt = av_packet_clone(pkt_src);
  
  if (!pkt) {
    return -1;
  }

  pkt1 = (AVPacketList *)av_malloc(sizeof(AVPacketList));
  if (!pkt1) {
    return -1;
  }
  pkt1->pkt  = *pkt;
  pkt1->next = NULL;

  if (q->nb_packets >= q->max_packets) {
    avpacket_queue_get(q, NULL, 0);
  }

  pthread_mutex_lock(&q->mutex);

  if (!q->last_pkt) {
    q->first_pkt = pkt1;
  } else {
    q->last_pkt->next = pkt1;
  }

  q->last_pkt = pkt1;
  q->nb_packets++;
  q->size += pkt1->pkt.size + sizeof(*pkt1);

  pthread_cond_signal(&q->cond);

  pthread_mutex_unlock(&q->mutex);
  return 0;
}

int avpacket_queue_get(AVPacketQueue *q, AVPacket *pkt, int block)
{
  AVPacketList *pkt1;
  int ret;

  pthread_mutex_lock(&q->mutex);

  for (;; ) {
    pkt1 = q->first_pkt;
    if (pkt1) {
      q->first_pkt = pkt1->next;
      if (!q->first_pkt) {
        q->last_pkt = NULL;
      }
      q->nb_packets--;
      q->size -= pkt1->pkt.size + sizeof(*pkt1);
      if (pkt)
        *pkt = pkt1->pkt;
      av_free(pkt1);
      ret = 1;
      break;
    } else if (!block) {
      ret = 0;
      break;
    } else {
      pthread_cond_wait(&q->cond, &q->mutex);
    }
  }
  pthread_mutex_unlock(&q->mutex);
  return ret;
}

size_t avpacket_queue_length(AVPacketQueue *q)
{
  size_t length;
  pthread_mutex_lock(&q->mutex);
  length = q->nb_packets;
  pthread_mutex_unlock(&q->mutex);
  return length;
}

uint64_t avpacket_queue_size(AVPacketQueue *q)
{
  uint64_t size;
  pthread_mutex_lock(&q->mutex);
  size = q->size;
  pthread_mutex_unlock(&q->mutex);
  return size;
}
