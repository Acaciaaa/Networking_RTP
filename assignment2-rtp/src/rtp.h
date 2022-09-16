#ifndef RTP_H
#define RTP_H

#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>

#define RTP_START 0
#define RTP_END   1
#define RTP_DATA  2
#define RTP_ACK   3

#define BUFFER_SIZE 2048
#define MAX_MSG 1472
#define MAX_WINDOW 100

typedef struct __attribute__ ((__packed__)) RTP_header {
    uint8_t type;       // 0: START; 1: END; 2: DATA; 3: ACK
    uint16_t length;    // Length of data; 0 for ACK, START and END packets
    uint32_t seq_num;
    uint32_t checksum;  // 32-bit CRC
} rtp_header_t;


typedef struct RTP_control_block {
    uint32_t window_size;
    // TODO: you can add your RTP-related fields here
    int seq_start; // first one who hasn't received ack
    int seq_now; // next buf waiting to be transfered
    char buf[MAX_WINDOW][BUFFER_SIZE];
    int buf_len[MAX_WINDOW];
    int ack;

    int rec_start;
    char rec_buf[MAX_WINDOW][BUFFER_SIZE];
    int rec_len[MAX_WINDOW];
    int rec_ack[MAX_WINDOW];

    int opt_ack[MAX_WINDOW];
} rcb_t;

extern rcb_t* rcb;

// different from the POSIX
int rtp_socket(uint32_t window_size);

int rtp_listen(int sockfd, int backlog);

int rtp_accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);

int rtp_bind(int sockfd, struct sockaddr *addr, socklen_t addrlen);

int rtp_connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen);

int rtp_close(int sockfd);

int rtp_sendto(int sockfd, const void *msg, int len, int seq, int flags, int type, const struct sockaddr *to, socklen_t tolen);

int rtp_recvfrom(int sockfd, void *buf, int len, int* seq, int flags, int* type, struct sockaddr *from, socklen_t *fromlen);

#endif //RTP_H
