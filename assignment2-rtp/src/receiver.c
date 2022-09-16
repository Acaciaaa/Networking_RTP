#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <netinet/in.h>

#include "util.h"
#include "rtp.h"

#define RECV_BUFFER_SIZE 32768  // 32KB

void writefile(FILE* f, int num){
    for(int i = 0; i < num; ++i)
        fwrite((void*)rcb->rec_buf[i], 1, rcb->rec_len[i], f);
}

void update(FILE* f){
    int num = 0;
    for(int i = 0; i < rcb->window_size; ++i){
        if(rcb->rec_ack[i] == 0)
            break;
        ++num;
    }
    rcb->rec_start += num;
    writefile(f, num);
    for(int i = 0; (i + num) < rcb->window_size; ++i){
        memset((void*)rcb->rec_buf[i], 0, BUFFER_SIZE);
        memcpy(rcb->rec_buf[i], rcb->rec_buf[i+num], rcb->rec_len[i+num]);
        rcb->rec_len[i] = rcb->rec_len[i+num];
        rcb->rec_ack[i] = rcb->rec_ack[i+num];
    }
    for(int i = 0; i < num; ++i){
        int j = rcb->window_size - 1- i;
        memset((void*)rcb->rec_buf[j], 0, BUFFER_SIZE);
        rcb->rec_len[j] = 0;
        rcb->rec_ack[j] = 0;
    }
}

void wait_send(FILE* f, const int sockfd){
    char tmp[BUFFER_SIZE];
    struct sockaddr_in sender;
    socklen_t addr_len = sizeof(struct sockaddr_in);
    int seq;
    int type;
    while(1)
    {
        memset((void*)tmp, 0, BUFFER_SIZE);
        int resp = rtp_recvfrom(sockfd, (void*)tmp, sizeof(tmp), &seq, 0, &type, (struct sockaddr*)&sender, &addr_len);
        if((uint8_t)type == RTP_START)
            continue;
        if((uint8_t)type == RTP_END){
            if(resp >= 0){
                rtp_sendto(sockfd, (void*)tmp, 0, seq, 0, RTP_ACK, (struct sockaddr*)&sender, addr_len);
                return;
            }
        }
        else{ // type=RTP_DATA
            if(resp >= 0){
                if(seq == rcb->rec_start){
                    rcb->rec_ack[0] = 1;
                    rcb->rec_len[0] = resp;
                    memcpy((void*)rcb->rec_buf[0], (void*)tmp, resp);
                    update(f);
                    rtp_sendto(sockfd, (void*)tmp, 0, rcb->rec_start, 0, RTP_ACK, (struct sockaddr*)&sender, addr_len);
                    continue;
                }
                for(int i = rcb->rec_start; i < (rcb->rec_start+rcb->window_size); ++i)
                    if(seq == i){
                        int j = i - rcb->rec_start;
                        rcb->rec_ack[j] = 1;
                        rcb->rec_len[j] = resp;
                        memcpy((void*)rcb->rec_buf[j], (void*)tmp, resp);
                        rtp_sendto(sockfd, (void*)tmp, 0, rcb->rec_start, 0, RTP_ACK, (struct sockaddr*)&sender, addr_len);
                        break;
                    }
            }
        }
    }
}

int receiver(char *receiver_port, int window_size, char* file_name) {

  char buffer[RECV_BUFFER_SIZE];

  // create rtp socket file descriptor
  int receiver_fd = rtp_socket(window_size);
  if (receiver_fd == 0) {
    perror("create rtp socket failed");
    exit(EXIT_FAILURE);
  }

  // create socket address
  // forcefully attach socket to the port
  struct sockaddr_in address;
  memset(&address, 0, sizeof(struct sockaddr_in));
  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(atoi(receiver_port));

  // bind rtp socket to address
  if (rtp_bind(receiver_fd, (struct sockaddr *)&address, sizeof(struct sockaddr))<0) {
    perror("bind failed");
    exit(EXIT_FAILURE);
  }

  int recv_bytes;
  struct sockaddr_in sender;
  socklen_t addr_len = sizeof(struct sockaddr_in);

  // listen to incoming rtp connection
  if(rtp_listen(receiver_fd, 1) < 0)
  {
    rtp_close(receiver_fd);
    return 0;
  }
  // accept the rtp connection
  rtp_accept(receiver_fd, (struct sockaddr*)&sender, &addr_len);

  FILE* f = fopen(file_name, "w");
  wait_send(f, receiver_fd);
  fclose(f);

  rtp_close(receiver_fd);

  return 0;
}

/*
 * main():
 * Parse command-line arguments and call receiver function
*/
int main(int argc, char **argv) {
    char *receiver_port;
    int window_size;
    char *file_name;

    if (argc != 4) {
        fprintf(stderr, "Usage: ./receiver [Receiver Port] [Window Size] [File Name]\n");
        exit(EXIT_FAILURE);
    }

    receiver_port = argv[1];
    window_size = atoi(argv[2]);
    file_name = argv[3];
    return receiver(receiver_port, window_size, file_name);
}
