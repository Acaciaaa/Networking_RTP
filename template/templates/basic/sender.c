#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "util.h"
#include "rtp.h"

#define SEND_BUFFER_SIZE 32768  // 32KB


int sender(char *receiver_ip, char* receiver_port, int window_size, char* message) {

  // create socket
  int sock = 0;
  if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
    perror("socket failed");
    exit(EXIT_FAILURE);
  }

  // create receiver address
  struct sockaddr_in receiver_addr;
  memset(&receiver_addr, 0, sizeof(receiver_addr));
  receiver_addr.sin_family = AF_INET;
  receiver_addr.sin_port = htons(atoi(receiver_port));

  // convert IPv4 or IPv6 addresses from text to binary form
  if(inet_pton(AF_INET, receiver_ip, &receiver_addr.sin_addr)<=0) {
    perror("address failed");
    exit(EXIT_FAILURE);
  }

  // send data
  char test_data[] = "Hello, world!\n";
  char send_buffer[SEND_BUFFER_SIZE];
  rtp_header_t *rtp = (rtp_header_t*)send_buffer;
  rtp->type = RTP_DATA;
  rtp->length = strlen(test_data);
  rtp->seq_num = 10;
  rtp->checksum = 0;
  memcpy((void*)send_buffer+ sizeof(rtp_header_t), test_data, strlen(test_data));
  rtp->checksum = compute_checksum((void *)send_buffer, sizeof(rtp_header_t) + strlen(test_data));
  sendto(sock, (void*)send_buffer, sizeof(rtp_header_t) + strlen(test_data), 0, (struct sockaddr *)&receiver_addr, sizeof(struct sockaddr));

  // close socket
  close(sock);

  return 0;
}


/*
 * main()
 * Parse command-line arguments and call sender function
*/
int main(int argc, char **argv) {
    char *receiver_ip;
    char *receiver_port;
    int window_size;
    char *message;

    if (argc != 5) {
        fprintf(stderr, "Usage: ./sender [Receiver IP] [Receiver Port] [Window Size] [Message]");
        exit(EXIT_FAILURE);
    }

    receiver_ip = argv[1];
    receiver_port = argv[2];
    window_size = atoi(argv[3]);
    message = argv[4];
    return sender(receiver_ip, receiver_port, window_size, message);
}