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
#include <sys/epoll.h>

#include "serial.h"

#define BUFSIZE 1024

int done = 0;

void term(int signum)
{ done = 1; }

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


    int epoll_fd = epoll_create(2);

    static struct epoll_event sock_ev;
    sock_ev.events = EPOLLIN | EPOLLPRI;
    sock_ev.data.fd = sock_fd;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, sock_fd, &sock_ev);

    static struct epoll_event serial_ev;
    serial_ev.events = EPOLLIN | EPOLLPRI;
    serial_ev.data.fd = serial_fd;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, serial_fd, &serial_ev);

    struct epoll_event *events = malloc(sizeof(struct epoll_event) * 2);

    int n_events;
    while(done == 0) {
        n_events = epoll_wait(epoll_fd, events, 2, -1); 

        for(int i=0; i < n_events; ++i) {
            if(
                (events[i].events & EPOLLERR) ||
                (events[i].events & EPOLLHUP) ||
                (!(events[i].events & EPOLLIN))
            ) {
                printf("epoll error\n");
                done = 1;
                break;
            } else if(events[i].data.fd == serial_fd) {
                if(copy(serial_fd, sock_fd, buf, BUFSIZE) == -1) {
                    done = 1;
                    break;
                }
            } else if(events[i].data.fd == sock_fd) {
                if(copy(sock_fd, serial_fd, buf, BUFSIZE) == -1) {
                    done = 1;
                    break;
                }
            }
        }
    }

    printf("exiting...");
    serial_flush(serial_fd);
    serial_close(serial_fd);
    close(sock_fd);
    free(events);
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
