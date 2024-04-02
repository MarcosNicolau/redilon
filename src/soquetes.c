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

struct HandleReadArgs
{
    int fd;
    void *handler_args;
    soquetes_Handler handler;
};

// we are using void* as a parameter, to allow multiple arguments in threads.
static void handleRead(void *args)
{
    int fd = ((struct HandleReadArgs *)args)->fd;
    soquetes_Handler handler = ((struct HandleReadArgs *)args)->handler;
    void *handler_args = ((struct HandleReadArgs *)args)->handler_args;
    paquetes_Packet *packet = paquetes_create(-1);
    if (packet == NULL)
        return;
    // op code and buffer size must always be explicit in the messages
    if (recv(fd, &(packet->op_code), sizeof(uint8_t), 0) == -1)
        return;
    if (recv(fd, &(packet->buffer->size), sizeof(uint32_t), 0) == -1)
        return;
    packet->buffer->stream = malloc(packet->buffer->size);
    if (packet->buffer->size != 0)
        recv(fd, packet->buffer->stream, packet->buffer->size, 0);
    handler(fd, packet->op_code, packet->buffer, handler_args);
    paquetes_free(packet);
    close(fd);
};

/**
 *
 * ============ lib functions ============
 *
 **/

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
int soquetes_acceptConnectionsAsync(int server_fd, int max_clients, soquetes_Handler handler, void *handler_args)
{
    // create epoll in edge-triggered mode
    int epoll_fd = epoll_create1(0);
    if (epoll_fd == -1)
        return -1;
    if (setNonBlocking(server_fd) == -1)
        return -1;
    struct epoll_event event;
    memset(&event, 0, sizeof(event));
    event.data.fd = server_fd;
    event.events = EPOLLIN | EPOLLET;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &event) == -1)
        return -1;

    // creates an array of size max_clients
    struct epoll_event *events = calloc(max_clients, sizeof(event));
    for (;;)
    {
        int number_fds = epoll_wait(epoll_fd, events, max_clients, -1);
        if (number_fds == -1)
        {
            free(events);
            return -1;
        }

        for (int i = 0; i < number_fds; i++)
        {
            {
                // error
                if ((events[i].events & EPOLLERR) || (events[i].events & EPOLLHUP) ||
                    (!(events[i].events & EPOLLIN)))
                {
                    close(events[i].data.fd);
                    continue;
                }
                // server socket
                if (events[i].data.fd == server_fd)
                {
                    // call accept as many times as we can
                    for (;;)
                    {
                        struct sockaddr client_addr;
                        socklen_t client_addrlen = sizeof(client_addr);
                        int client = accept(server_fd, &client_addr, &client_addrlen);
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
                    };
                }
                // handle client
                else
                {
                    struct HandleReadArgs args;
                    args.fd = events[i].data.fd;
                    args.handler = handler;
                    args.handler_args = handler_args;
                    handleRead(&args);
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, events[i].data.fd, NULL);
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
int soquetes_acceptConnectionsOnDemand(int server_fd, soquetes_Handler handler, void *handler_args)
{
    for (;;)
    {

        pthread_t thread;
        int client;
        struct sockaddr client_addr;
        socklen_t client_addrlen = sizeof(client_addr);
        client = accept(server_fd, &client_addr, &client_addrlen);
        if (client == -1)
            continue;
        // dynamically allocating memory to ensure its memory persists beyond the current iteration
        struct HandleReadArgs *args = malloc(sizeof(struct HandleReadArgs));
        if (args == NULL)
        {
            close(client);
            continue;
        }
        args->fd = client;
        args->handler = handler;
        args->handler_args = handler_args;
        pthread_create(&thread, NULL, (void *)handleRead, args);
        pthread_detach(thread);
    };
};

void soquetes_sendToClient(int client_fd, paquetes_Packet *packet)
{
    send(client_fd, paquetes_serialize(packet), paquetes_getSize(packet), 0);
    paquetes_free(packet);
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

void soquetes_sendToServer(int server_fd, paquetes_Packet *packet, soquetes_Handler handler, void *handler_args)
{
    send(server_fd, paquetes_serialize(packet), paquetes_getSize(packet), 0);
    struct HandleReadArgs args;
    args.fd = server_fd;
    args.handler = handler;
    args.handler_args = handler_args;
    handleRead(&args);
    paquetes_free(packet);
}