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

int main(int argc, char *argv[])
{
    struct sigaction action;
    memset(&action, 0, sizeof(struct sigaction));
    action.sa_handler = term;
    sigaction(SIGTERM, &action, NULL);
    sigaction(SIGINT, &action, NULL);

    int sock_fd = 0, serial_fd = 0, n = 0, len = 0;
    char rcv_buf[BUFSIZE];
    char snd_buf[BUFSIZE];
    struct sockaddr_in serv_addr; 

    if(argc != 4) {
        printf("\n Usage: %s <serial> <server> <port> \n", argv[0]);
        return 1;
    } 

    memset(rcv_buf, 0, sizeof(rcv_buf));
    memset(snd_buf, 0, sizeof(snd_buf));

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

    while(done == 0) {
        n = read(serial_fd, snd_buf, sizeof(snd_buf));
        if(n == - 1 && errno != EAGAIN) {
            break;
        }

        if(n > 0) {
            n = write(sock_fd, snd_buf, n);
            if(n == -1) {
                break;
            }
        }

        n = read(sock_fd, rcv_buf, sizeof(rcv_buf));
        if(n == - 1 && errno != EAGAIN) {
            break;
        }

        if(n > 0) {
            len = n;
            n = write(serial_fd, rcv_buf, n);
            if(n == -1 || n != len) {
                break;
            }
        }

        usleep(1000);
    }

    printf("exiting...");
    serial_flush(serial_fd);
    serial_close(serial_fd);
    close(sock_fd);
    printf("done.\n");

    return 0;
}
