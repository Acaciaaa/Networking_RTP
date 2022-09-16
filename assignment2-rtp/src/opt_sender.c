#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>

#include "util.h"
#include "rtp.h"

int update()
{
    int num = 0;
    for(int i = 0; i < rcb->window_size; ++i){
        if(rcb->opt_ack[i] == 0)
            break;
        ++num;
    }
    for(int i = 0; (i + num) < rcb->window_size; ++i)
    {
        memset((void*)rcb->buf[i], 0, BUFFER_SIZE);
        memcpy(rcb->buf[i], rcb->buf[i+num], rcb->buf_len[i+num]);
        rcb->buf_len[i] = rcb->buf_len[i+num];
        rcb->opt_ack[i] = rcb->opt_ack[i+num];
    }
    for(int i = 0; i < num; ++i)
    {
        int j = rcb->window_size - 1 - i;
        memset((void*)rcb->buf[j], 0, BUFFER_SIZE);
        rcb->buf_len[j] = 0;
        rcb->opt_ack[j] = 0;
    }
    rcb->seq_start += num;
    return num;
}

void readfile(FILE* f, int num)
{
    const int chunksize = MAX_MSG-sizeof(struct RTP_header);
    char tmp[chunksize];
    int start = (int)(rcb->window_size) - num;
    for(int i = 0; i < num; ++i)
    {
        if(feof(f))
            break;
        int count=fread((void*)tmp,1,chunksize,f);

        if(count > 0)
        {
            memcpy((void*)(rcb->buf[start+i]), (void*)tmp, count);
            rcb->buf_len[start+i] = count;
            memset((void*)tmp, 0, chunksize);
        }
    }
}

void send_wait(FILE* f, int s, const int sockfd, const struct sockaddr *to, socklen_t tolen)
{
    if(rcb->buf_len[0] == 0)
        return; // completely finished

    for(int i = s; i < rcb->window_size; ++i)
    {
        if(rcb->buf_len[i] == 0)
            break;
        if(rcb->opt_ack[i] == 0)
            rtp_sendto(sockfd, (void*)(rcb->buf[i]), rcb->buf_len[i], rcb->seq_start+i, 0, RTP_DATA, to, tolen);
        if(rcb->seq_start+i == rcb->seq_now)
            rcb->seq_now++;
    }

    fd_set fds;
    struct timeval timeout= {0,500};
    int maxfdp = sockfd + 1;

    while(1)
    {
        FD_ZERO(&fds);
        FD_SET(sockfd,&fds);
        switch(select(maxfdp,&fds,NULL,NULL,&timeout))
        {
        case -1:
            exit(-1);
            break;
        case 0:
            if(f)
                send_wait(f, 0, sockfd, to, tolen);
            else
                send_wait(NULL, 0, sockfd, to, tolen);
            return;
        default:
            if(FD_ISSET(sockfd,&fds))
            {
                char tmp[BUFFER_SIZE];
                struct sockaddr_in sender;
                socklen_t addr_len = sizeof(struct sockaddr_in);
                int resp;
                int type;
                if(rtp_recvfrom(sockfd, (void*)tmp, sizeof(tmp), &resp, 0, &type, (struct sockaddr*)&sender, &addr_len) < 0)
                    return;

                if(resp == rcb->seq_start){
                    rcb->opt_ack[0] = 1;
                    int num = update();
                    if(f)
                    {
                        readfile(f, num);
                        send_wait(f, rcb->window_size-num, sockfd, to, tolen);
                    }
                    else
                        send_wait(NULL, rcb->window_size-num, sockfd, to, tolen);
                    return;
                }
                for(int i = rcb->seq_start + 1; i < rcb->seq_now; ++i)
                {
                    if(resp == i)
                    {
                        rcb->opt_ack[i-rcb->seq_start] = 1;
                        break;
                    }
                }
            }
        }
    }
}

void end(const int sockfd, const struct sockaddr *to, socklen_t tolen)
{
    char buffer[BUFFER_SIZE];
    rtp_sendto(sockfd, (void*)buffer, 0, rcb->seq_now, 0, RTP_END, to, tolen);

    fd_set fds;
    struct timeval timeout= {0,500};
    int maxfdp = sockfd + 1;
waitend:
    FD_ZERO(&fds);
    FD_SET(sockfd,&fds);
    switch(select(maxfdp,&fds,NULL,NULL,&timeout))
    {
    case -1:
        exit(-1);
        break;
    case 0:
        return;
    default:
        if(FD_ISSET(sockfd,&fds))
        {
            struct sockaddr_in sender;
            socklen_t addr_len = sizeof(struct sockaddr_in);
            int seq;
            int type;
            if (rtp_recvfrom(sockfd, (void *)buffer, sizeof(buffer), &seq, 0, &type, (struct sockaddr*)&sender, &addr_len) < 0)
            {
                perror("end receive error");
                return;
            }
            if (seq != rcb->seq_now)
                goto waitend;
        }
    }
}

int sender(char *receiver_ip, char* receiver_port, int window_size, char* message)
{
    // create socket
    int sock = 0;
    if ((sock = rtp_socket(window_size)) < 0)
    {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // create receiver address
    struct sockaddr_in receiver_addr;
    memset(&receiver_addr, 0, sizeof(receiver_addr));
    receiver_addr.sin_family = AF_INET;
    receiver_addr.sin_port = htons(atoi(receiver_port));

    // convert IPv4 or IPv6 addresses from text to binary form
    if(inet_pton(AF_INET, receiver_ip, &receiver_addr.sin_addr)<=0)
    {
        perror("address failed");
        exit(EXIT_FAILURE);
    }

    // connect to server
    if (rtp_connect(sock, (struct sockaddr *)&receiver_addr, sizeof(struct sockaddr)) < 0)
    {
        end(sock, (struct sockaddr*)&receiver_addr, sizeof(struct sockaddr));
        rtp_close(sock);
        return 0;
    }
    // send data
    // char test_data[] = "Hello, world!\n";
    // TODO: if message is filename, open the file and send its content
    // rtp_sendto(sock, (void *)test_data, strlen(test_data), 0, (struct sockaddr*)&receiver_addr, sizeof(struct sockaddr));
    FILE *f = fopen(message, "r");
    if (f == NULL)
    {
        memcpy((void *)rcb->buf[0], message, strlen(message));
        rcb->buf_len[0] = strlen(message);
        send_wait(NULL, 0, sock, (struct sockaddr*)&receiver_addr, sizeof(struct sockaddr));
    }
    else
    {
        readfile(f, (int)window_size);
        send_wait(f, 0, sock, (struct sockaddr*)&receiver_addr, sizeof(struct sockaddr));
        fclose(f);
    }

    // send END
    end(sock, (struct sockaddr*)&receiver_addr, sizeof(struct sockaddr));

    // close rtp socket
    rtp_close(sock);

    return 0;
}



/*
 * main()
 * Parse command-line arguments and call sender function
*/
int main(int argc, char **argv)
{
    char *receiver_ip;
    char *receiver_port;
    int window_size;
    char *message;

    if (argc != 5)
    {
        fprintf(stderr, "Usage: ./sender [Receiver IP] [Receiver Port] [Window Size] [message]");
        exit(EXIT_FAILURE);
    }

    receiver_ip = argv[1];
    receiver_port = argv[2];
    window_size = atoi(argv[3]);
    message = argv[4];
    return sender(receiver_ip, receiver_port, window_size, message);
}
