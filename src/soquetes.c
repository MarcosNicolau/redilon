#include "stdlib.h"
#include "stdio.h"
#include "errno.h"
#include "string.h"
#include "sys/socket.h"
#include "sys/epoll.h"
#include "unistd.h"
#include "netdb.h"
#include "fcntl.h"
#include "commons/log.h"
#include "pthread.h"
#include "./paquetes.h"
#include "./soquetes.h"

// private fns
static int setNonBlocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
        return -1;
    return 0;
}

enum SocketType
{
    CLIENT,
    SERVER,
};

/**
 *
 * @param host set is a NULL if you wish it to be your computer host.
 * @returns a linked list with several hosts for us to create a socket and bind/connect the socket to or `NULL` on error.
 */
static struct addrinfo *getAddrInfo(char *host, char *port, enum SocketType type)
{
    // restrictions imposed to getaddrinfo
    struct addrinfo hints;
    struct addrinfo *addrInfo;
    memset(&hints, 0, sizeof(hints));
    // don't specify any specific address, so that the socket is accesible through multiple protocols (IPV4, IPV6)
    hints.ai_family = AF_UNSPEC;
    // tcp streams
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = 0;
    // specify this flag only on server sockets, so that the returned lists includes sockets suitable for binding and accepting connections
    if (type == SERVER)
        hints.ai_flags = AI_PASSIVE;
    hints.ai_canonname = NULL;

    int status = getaddrinfo(host, port, &hints, &addrInfo);
    if (status != 0)
        return NULL;

    return addrInfo;
}

struct HandleReadThreadArgs
{
    int fd;
    void *args;
    void (*onClientClosed)(int client_fd, void *args);
    soquetes_Handler requestHandler;
};

// we are using void* as a parameter, to allow multiple arguments in threads.
static void handleReadThread(void *_args)
{
    struct HandleReadThreadArgs *args = _args;

    int fd = args->fd;
    soquetes_Handler requestHandler = args->requestHandler;
    void *handlerArgs = args->args;

    // in the threaded version, to keep the connection alive we need this loop
    // otherwise the thread will die
    int res = 0;
    // until connection gets closed
    while (res != -1)
    {
        res = soquetes_read(args->fd, args->requestHandler, handlerArgs);
    }

    if (args->onClientClosed != NULL)
        args->onClientClosed(fd, handlerArgs);
};

/**
 *
 * ============ lib functions ============
 *
 **/

/**
 * @param requestHandler pass NULL if you don't expect a response from the server.
 * @returns `-1` when client is closed
 */
int soquetes_read(int fd, soquetes_Handler requestHandler, void *args)
{
    paquetes_Packet *packet = paquetes_create(-1);
    if (packet == NULL)
        return 0;
    // op code and buffer size must always be explicit in the messages
    int bytes_read;
    bytes_read = recv(fd, &(packet->op_code), sizeof(uint8_t), 0);
    bytes_read = recv(fd, &(packet->buffer->size), sizeof(uint32_t), 0);
    packet->buffer->stream = malloc(packet->buffer->size);
    if (packet->buffer->size != 0)
        bytes_read = recv(fd, packet->buffer->stream, packet->buffer->size, 0);

    // no data was sent
    if (bytes_read == -1)
        return 0;
    // connection closed
    if (bytes_read == 0)
        return -1;

    // everything alright call the requestHandler
    requestHandler(fd, packet->op_code, packet->buffer, args);
    paquetes_free(packet);
    return 0;
};

/**
 * Creates a tcp server using sockets.
 * @returns the socket file descriptor or `-1` if it fails.
 */
int soquetes_createTcpServer(char *port, unsigned int queue_size)
{
    struct addrinfo *addrInfo = getAddrInfo(NULL, port, SERVER);
    if (addrInfo == NULL)
        return -1;

    int fileDescriptor;
    struct addrinfo *addr;
    /* getaddrinfo() returns a list of address structures.
             Try each address until we successfully bind(2).
             If socket(2) (or bind(2)) fails, we (close the socket
             and) try the next address. */
    for (addr = addrInfo; addr != NULL; addr = addr->ai_next)
    {
        fileDescriptor = socket(addr->ai_family, addr->ai_socktype,
                                addr->ai_protocol);
        if (fileDescriptor == -1)
            continue;

        if (bind(fileDescriptor, addr->ai_addr, addr->ai_addrlen) == 0)
            break; /* Success */

        // this means that the we could not bind the socket, so close it and try next one.
        close(fileDescriptor);
    }

    freeaddrinfo(addrInfo);

    if (addr == NULL)
        return -1;

    int listening = listen(fileDescriptor, queue_size);
    if (listening == -1)
        return -1;

    return fileDescriptor;
}

/**
 * accept connections using an async non blocking io mechanism with epoll
 *
 * @returns `-1` if there is an error
 */
int soquetes_acceptConnectionsAsync(soquetes_AsyncServerConf *conf)
{
    // create epoll in edge-triggered mode
    int epoll_fd = epoll_create1(0);
    if (epoll_fd == -1)
        return -1;

    *conf->epoll_fd = epoll_fd;

    if (setNonBlocking(conf->server_fd) == -1)
        return -1;
    struct epoll_event event;
    memset(&event, 0, sizeof(event));
    event.data.fd = conf->server_fd;
    event.events = EPOLLIN | EPOLLET;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, conf->server_fd, &event) == -1)
        return -1;

    // creates an array of size max_clients
    struct epoll_event *events = calloc(conf->max_clients, sizeof(event));
    for (;;)
    {
        int number_fds = epoll_wait(epoll_fd, events, conf->max_clients, -1);
        if (number_fds == -1)
        {
            free(events);
            return -1;
        }

        for (int i = 0; i < number_fds; i++)
        {
            {
                // error
                if ((events[i].events & EPOLLERR) ||
                    (!(events[i].events & EPOLLIN)))
                {
                    continue;
                }
                // server socket
                if (events[i].data.fd == conf->server_fd)
                {
                    // call accept as many times as we can
                    for (;;)
                    {
                        struct sockaddr client_addr;
                        socklen_t client_addrlen = sizeof(client_addr);
                        int client = accept(conf->server_fd, &client_addr, &client_addrlen);
                        if (client == -1)
                        {
                            if (errno == EAGAIN || errno == EWOULDBLOCK)
                                // we processed all of the connections
                                break;
                            continue;
                        }
                        setNonBlocking(client);
                        event.events = EPOLLIN | EPOLLOUT;
                        event.data.fd = client;
                        epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client, &event);
                        if (conf->onNewConnection != NULL)
                            conf->onNewConnection(client, conf->args);
                    };
                }
                // handle client
                else
                {
                    int result = soquetes_read(events[i].data.fd, conf->requestHandler, conf->args);
                    if (result == -1)
                    {
                        if (conf->onConnectionClosed != NULL)
                            conf->onConnectionClosed(events[i].data.fd, conf->args);
                    }
                }
            }
        }
    };
};

/**
 * accept connections using an on-demand mechanism (i.e creates one thread per client)
 *
 * @note there is no limitation on the amount of threads created
 *
 * @returns `-1` if there is an error.
 */
int soquetes_acceptConnectionsOnDemand(soquetes_OnDemandServerConf *conf)
{
    for (;;)
    {

        pthread_t thread;
        int client;
        struct sockaddr client_addr;
        socklen_t client_addrlen = sizeof(client_addr);
        client = accept(conf->server_fd, &client_addr, &client_addrlen);
        if (client == -1)
            continue;
        // dynamically allocating memory to ensure its memory persists beyond the current iteration
        struct HandleReadThreadArgs *args = malloc(sizeof(struct HandleReadThreadArgs));
        if (args == NULL)
        {
            close(client);
            continue;
        }
        if (conf->onNewConnection != NULL)
            conf->onNewConnection(client, conf->args);

        args->fd = client;
        args->requestHandler = conf->requestHandler;
        args->args = conf->args;
        args->onClientClosed = conf->onConnectionClosed;
        pthread_create(&thread, NULL, (void *)handleReadThread, args);
        pthread_detach(thread);
    };
};

/**
 * @returns `-1` if theres is an error
 */
int soquetes_sendToClient(int client_fd, paquetes_Packet *packet, int should_free)
{
    int res = send(client_fd, paquetes_serialize(packet), paquetes_getSize(packet), 0);
    if (should_free)
        free(packet);
    return res;
}

/**
 * if you are accepting connection `on-demand` then ignore the `epoll_fd` by passing a `-1`
 *
 * if you are using an `async` server, be aware that a connection will be deleted from epoll if all its file descriptors have been closed.
 * So, if you have duplicated a file descriptor via dup(2), dup2(2), fcntl(2) F_DUPFD, or fork(2), then you need to make sure to close all the fds.
 * To prevent this, you should pass the `epoll_fd` to close all connections.
 */
void soquetes_closeClientConn(int client_fd, int epoll_fd)
{
    close(client_fd);
    if (epoll_fd != -1)
        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);
}

/**
 * @returns server file descriptor if connection success or `-1` in case of an error.
 */
int soquetes_connectToTcpServer(char *host, char *port)
{
    struct addrinfo *serverInfo = getAddrInfo(host, port, CLIENT);
    if (serverInfo == NULL)
        return -1;

    int fileDescriptor;
    struct addrinfo *addr;
    /* getaddrinfo() returns a list of address structures.
              Try each address until we successfully connect(2).
              If socket(2) (or connect(2)) fails, we (close the socket
              and) try the next address. */
    for (addr = serverInfo; addr != NULL; addr = addr->ai_next)
    {
        fileDescriptor = socket(addr->ai_family, addr->ai_socktype,
                                addr->ai_protocol);
        if (fileDescriptor == -1)
            continue;

        if (connect(fileDescriptor, addr->ai_addr, addr->ai_addrlen) != -1)
            break; /* Success */

        close(fileDescriptor);
    }

    freeaddrinfo(serverInfo);

    /* No address succeeded */
    if (addr == NULL)
        return -1;

    return fileDescriptor;
}

/**
 * @param requestHandler pass NULL if you don't expect a response from the server so the app does not get stuck waiting to read.
 * @returns `-1` if the connection closed and failed
 */
int soquetes_sendToServer(int server_fd, paquetes_Packet *packet, soquetes_Handler requestHandler, void *handler_args)
{
    int result = send(server_fd, paquetes_serialize(packet), paquetes_getSize(packet), 0);
    paquetes_free(packet);
    if (requestHandler == NULL || result == -1)
        return result;
    int read = soquetes_read(server_fd, requestHandler, handler_args);
    return read;
}

void soquetes_closeServerConn(int server_fd)
{
    close(server_fd);
}