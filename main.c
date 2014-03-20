#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h> 
#include <fcntl.h>
#include <signal.h>

#include "serial.h"

#define BUFSIZE 1024

int done = 0;

void term(int signum)
{
    done = 1;
}

int copy(int src_fd, int dest_fd, char* buf, size_t buf_len);

int main(int argc, char *argv[])
{
    struct sigaction action;
    memset(&action, 0, sizeof(struct sigaction));
    action.sa_handler = term;
    sigaction(SIGTERM, &action, NULL);
    sigaction(SIGINT, &action, NULL);

    int sock_fd = 0, serial_fd = 0;
    char buf[BUFSIZE];
    struct sockaddr_in serv_addr; 

    if(argc != 4) {
        printf("\n Usage: %s <serial> <server> <port> \n", argv[0]);
        return 1;
    } 

    printf("Connecting to %s:%s...", argv[2], argv[3]);
    if((sock_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("\n Error: Could not create socket \n");
        return 1;
    } 

    memset(&serv_addr, 0, sizeof(serv_addr)); 

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons((uint16_t)atoi(argv[3])); 

    if(inet_pton(AF_INET, argv[2], &serv_addr.sin_addr) <= 0) {
        printf("\n Error: inet_pton error occured\n");
        return 1;
    } 

    if(connect(sock_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
       printf("\n Error: Connect failed \n");
       return 1;
    } 
    fcntl(sock_fd, F_SETFL, O_NONBLOCK);
    printf("done.\n");

    printf("Connecting to %s...", argv[1]);
    serial_fd = serial_init(argv[1], 9600);
    if(serial_fd < 0) {
        printf("\n Error: Connect to serial failed \n");
        return 1;
    }
    serial_flush(serial_fd);
    usleep(1000000);
    printf("done.\n");



    fd_set readset;
    int maxfd = sock_fd;
    if (serial_fd > sock_fd) maxfd = serial_fd;

    while(done == 0) {
        FD_ZERO(&readset);

        // Add all of the interesting fds to readset
        FD_SET(sock_fd, &readset);
        FD_SET(serial_fd, &readset);

        // Wait until one or more fds are ready to read
        select(maxfd+1, &readset, NULL, NULL, NULL);

        if(FD_ISSET(serial_fd, &readset)) {
            if(copy(serial_fd, sock_fd, buf, BUFSIZE) == -1) {
                break;
            }
        }

        if(FD_ISSET(sock_fd, &readset)) {
            if(copy(sock_fd, serial_fd, buf, BUFSIZE) == -1) {
                break;
            }
        }
    }

    printf("exiting...");
    serial_flush(serial_fd);
    serial_close(serial_fd);
    close(sock_fd);
    printf("done.\n");

    return 0;
}

int copy(int src_fd, int dest_fd, char* buf, size_t buf_len)
{
    int n = read(src_fd, buf, buf_len);

    if(n == -1 && errno == EAGAIN) {
        n = 0;
    } else if(n > 0) {
        n = write(dest_fd, buf, n);
    }

    return n;
}
