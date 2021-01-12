#pragma once
#include <iostream>

extern "C" {
#include <libavformat/avformat.h>
}

typedef struct AVPacketQueue {
    AVPacketList *first_pkt, *last_pkt;
    size_t nb_packets;
    size_t max_packets;
    uint64_t size;
    int abort_request;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
} AVPacketQueue;

void avpacket_queue_init(AVPacketQueue *q, size_t max_packets);
void avpacket_queue_flush(AVPacketQueue *q);
void avpacket_queue_end(AVPacketQueue *q);
int avpacket_queue_put(AVPacketQueue *q, const AVPacket *pkt_src);
int avpacket_queue_get(AVPacketQueue *q, AVPacket *pkt, int block);
size_t avpacket_queue_length(AVPacketQueue *q);
uint64_t avpacket_queue_size(AVPacketQueue *q);
