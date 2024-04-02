#ifndef soquetes_H
#define soquetes_H

#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netdb.h>
#include <fcntl.h>
#include <pthread.h>
#include "./paquetes.h"

// Structures
typedef void (*soquetes_Handler)(uint8_t client_fd, uint8_t operation, paquetes_Buffer *buffer, void *args);

/**
 * @param onAcceptClientError runs every time a new connection with a client cannot be established(i.e accept() epoll_ctl(EPOLL_CTL_ADD))
 */
typedef struct soquetes_AcceptConnectionsAsyncConf
{
    int server_fd;
    int max_clients;
    void (*onAcceptClientError)(int _errno);
} soquetes_AcceptConnectionsAsyncConf;

// Function prototypes
int soquetes_createTcpServer(char *port, unsigned int queue_size);
int soquetes_acceptConnectionsAsync(int server_fd, int max_clients, soquetes_Handler handler, void *handler_args);
int soquetes_acceptConnectionsOnDemand(int server_fd, soquetes_Handler handler, void *handler_args);
void soquetes_sendToClient(int client_fd, paquetes_Packet *packet);
int soquetes_connectToTcpServer(char *host, char *port);
void soquetes_sendToServer(int server_fd, paquetes_Packet *packet, soquetes_Handler handler, void *handler_args);

#endif // soquetes_H
