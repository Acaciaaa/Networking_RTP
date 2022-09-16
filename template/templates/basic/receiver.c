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


#define RECV_BUFFER_SIZE 32768  // 32KB


int receiver(char *receiver_port, int window_size, char* file_name) {

    char buffer[RECV_BUFFER_SIZE];

    // create socket file descriptor
    int receiver_fd;
    if ((receiver_fd = socket(AF_INET, SOCK_DGRAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // create socket address
    struct sockaddr_in address;
    memset(&address, 0, sizeof(struct sockaddr_in));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(atoi(receiver_port));

    // bind socket to address
    if (bind(receiver_fd, (struct sockaddr *)&address, sizeof(struct sockaddr))<0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    int recv_bytes;
    struct sockaddr_in sender;
    socklen_t addr_len = sizeof(struct sockaddr_in);
    // receive packet
    if ((recv_bytes = recvfrom(receiver_fd, buffer, sizeof(buffer), 0, (struct sockaddr*)&sender, &addr_len)) < 0) {
        perror("receive error");
        exit(EXIT_FAILURE);
    }
    buffer[recv_bytes] = '\0';
    // extract header and payload
    rtp_header_t *rtp = (rtp_header_t *)buffer;
    char *msg = &buffer[sizeof(rtp_header_t)];

    // verify checksum
    uint32_t pkt_checksum = rtp->checksum;
    rtp->checksum = 0;
    uint32_t computed_checksum = compute_checksum((void *)buffer, recv_bytes);
    if (pkt_checksum != computed_checksum) {
        perror("checksums not match");
        exit(EXIT_FAILURE);
    }

    // print payload
    fprintf(stdout, "receive msg: %s", msg);

    // close socket
    close(receiver_fd);
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
