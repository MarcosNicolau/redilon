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
 */
typedef struct soquetes_AsyncServerConf
{
    int server_fd;
    int *epoll_fd;
    int max_clients;
    void *args;
    /**
     * gets fired when connected client sends data.
     */
    soquetes_Handler requestHandler;
    /**
     * gets fired when client unexpectedly closes the connection
     *
     * @note
     * if you close the connection yourself this method will not get called.
     */
    void (*onConnectionClosed)(int client_fd, void *args);
    /**
     * gets fired whenever a client makes the initial connection to the socket.
     */
    void (*onNewConnection)(int client_fd, void *args);
} soquetes_AsyncServerConf;

typedef struct soquetes_OnDemandServerConf
{
    int server_fd;
    void *args;
    soquetes_Handler requestHandler;
    void (*onConnectionClosed)(int client_fd, void *args);
    void (*onNewConnection)(int client_fd, void *args);
} soquetes_OnDemandServerConf;

// Function prototypes
int soquetes_read(int fd, soquetes_Handler requestHandler, void *args);
// server
int soquetes_createTcpServer(char *port, unsigned int queue_size);
int soquetes_acceptConnectionsAsync(soquetes_AsyncServerConf *conf);
int soquetes_acceptConnectionsOnDemand(soquetes_OnDemandServerConf *conf);
int soquetes_sendToClient(int client_fd, paquetes_Packet *packet, int should_free);
void soquetes_closeClientConn(int client_fd, int epoll_fd);
// client
int soquetes_connectToTcpServer(char *host, char *port);
int soquetes_sendToServer(int server_fd, paquetes_Packet *packet, soquetes_Handler requestHandler, void *handler_args);
void soquetes_closeServerConn(int server_fd);

#endif // soquetes_H
