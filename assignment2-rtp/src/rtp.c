#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>

#include "rtp.h"
#include "util.h"

rcb_t* rcb = NULL;
void rcb_init(uint32_t window_size)
{
    if (rcb == NULL)
    {
        rcb = (rcb_t *) calloc(1, sizeof(rcb_t));
    }
    else
    {
        perror("The current version of the rtp protocol only supports a single connection");
        exit(EXIT_FAILURE);
    }
    rcb->window_size = window_size;

    rcb->seq_start = 0;
    rcb->seq_now = 0;
    memset((void*)(rcb->buf_len), 0, MAX_WINDOW);
    rcb->ack = -1;

    rcb->rec_start = 0;
    memset((void*)(rcb->rec_len), 0, MAX_WINDOW);
    memset((void*)(rcb->rec_ack), 0, MAX_WINDOW);

    memset((void*)(rcb->opt_ack), 0, MAX_WINDOW);
    // TODO: you can initialize your RTP-related fields here
}

/*********************** Note ************************/
/* RTP in Assignment 2 only supports single connection.
/* Therefore, we can initialize the related fields of RTP when creating the socket.
/* rcb is a global varialble, you can directly use it in your implementatyion.
/*****************************************************/
int rtp_socket(uint32_t window_size)
{
    rcb_init(window_size);
    // create UDP socket
    return socket(AF_INET, SOCK_DGRAM, 0);
}


int rtp_bind(int sockfd, struct sockaddr *addr, socklen_t addrlen)
{
    return bind(sockfd, addr, addrlen);
}


int rtp_listen(int sockfd, int backlog)
{
    // TODO: listen for the START message from sender and send back ACK
    // In standard POSIX API, backlog is the number of connections allowed on the incoming queue.
    // For RTP, backlog is always 1
    char buffer[BUFFER_SIZE];
    struct sockaddr_in sender;
    socklen_t addr_len = sizeof(struct sockaddr_in);
    int seq;
    int type;
    if (rtp_recvfrom(sockfd, (void *)buffer, sizeof(buffer), &seq, 0, &type, (struct sockaddr*)&sender, &addr_len) < 0)
    {
        perror("listen receive error");
        return -1;
    }
    memset((void*)buffer, 0, BUFFER_SIZE);

    if((uint8_t)type == RTP_START)
        rtp_sendto(sockfd, (void*)buffer, 0, seq, 0, RTP_ACK, (struct sockaddr*)&sender, addr_len);
    else
        perror("listen receive type error");

    return 1;
}


int rtp_accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen)
{
    // Since RTP in Assignment 2 only supports one connection,
    // there is no need to implement accpet function.
    // You don’t need to make any changes to this function.
    return 1;
}

int rtp_connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen)
{
    // TODO: send START message and wait for its ACK
    srand((int)time(NULL));
    int seq_rand = rand();
    char buffer[BUFFER_SIZE];

    rtp_sendto(sockfd, (void*)buffer, 0, seq_rand, 0, RTP_START, addr, addrlen);

    fd_set fds;
    struct timeval timeout= {0,500};
    int maxfdp = sockfd + 1;
    FD_ZERO(&fds);
    FD_SET(sockfd,&fds);
    switch(select(maxfdp,&fds,NULL,NULL,&timeout))
    {
    case -1:
        exit(-1);
        break;
    case 0:
        return -1;
    default:
        if(FD_ISSET(sockfd,&fds))
        {
            struct sockaddr_in sender;
            socklen_t addr_len = sizeof(struct sockaddr_in);
            int seq;
            int type;
            if (rtp_recvfrom(sockfd, (void *)buffer, sizeof(buffer), &seq, 0, &type, (struct sockaddr*)&sender, &addr_len) < 0)
            {
                perror("connect receive error");
                return -1;
            }
        }
    }

    return 1;
}

int rtp_close(int sockfd)
{
    return close(sockfd);
}


int rtp_sendto(int sockfd, const void *msg, int len, int seq, int flags, int type, const struct sockaddr *to, socklen_t tolen)
{
    // TODO: send message

    // Send the first data message sample
    char buffer[BUFFER_SIZE];
    rtp_header_t* rtp = (rtp_header_t*)buffer;
    rtp->length = len;
    rtp->checksum = 0;
    rtp->seq_num = (uint32_t)seq;
    rtp->type = type;
    memcpy((void *)buffer+sizeof(rtp_header_t), msg, len);
    rtp->checksum = compute_checksum((void *)buffer, sizeof(rtp_header_t) + len);

    int sent_bytes = sendto(sockfd, (void*)buffer, sizeof(rtp_header_t) + len, flags, to, tolen);
    if (sent_bytes != (sizeof(struct RTP_header) + len))
    {
        perror("send error");
        exit(EXIT_FAILURE);
    }
    return 1;

}

int rtp_recvfrom(int sockfd, void *buf, int len, int* seq, int flags, int* type, struct sockaddr *from, socklen_t *fromlen)
{
    // TODO: recv message

    char buffer[2048];
    int recv_bytes = recvfrom(sockfd, buffer, 2048, flags, from, fromlen);
    if (recv_bytes < 0)
    {
        perror("receive error");
        exit(EXIT_FAILURE);
    }
    buffer[recv_bytes] = '\0';

    // extract header
    rtp_header_t *rtp = (rtp_header_t *)buffer;

    // verify checksum
    uint32_t pkt_checksum = rtp->checksum;
    rtp->checksum = 0;
    uint32_t computed_checksum = compute_checksum(buffer, recv_bytes);
    if (pkt_checksum != computed_checksum)
    {
        perror("checksums not match");
        return -1;
    }
    *seq = (int)rtp->seq_num;
    *type = (int)rtp->type;

    memcpy(buf, buffer+sizeof(rtp_header_t), rtp->length);

    return rtp->length;
}
