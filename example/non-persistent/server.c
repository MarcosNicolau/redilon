#include <stdlib.h>
#include <stdio.h>
#include "arpa/inet.h"
#include "./proto.h"
#include "../../src/paquetes.h"
#include "../../src/soquetes.h"

// just arbitrary values
#define QUEUE_SIZE 10
#define PORT "8000"
#define MAX_CLIENTS 100

void getClientIp(int client_fd, char *ip)
{
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);

    if (getpeername(client_fd, (struct sockaddr *)&client_addr, &addr_len) == -1)
        return;
    inet_ntop(AF_INET, &(client_addr.sin_addr), ip, INET_ADDRSTRLEN);
}

struct ConnectionArgs
{
    // this is necessary to close the client in the async mechanism
    int epoll_fd;
};

void handleRequest(uint8_t client_fd, uint8_t op_code, paquetes_Buffer *buffer, void *args)
{
    char ip[INET6_ADDRSTRLEN];
    getClientIp(client_fd, ip);
    printf("Handling request from %s\n", ip);
    struct ConnectionArgs *my_args = args;

    switch (op_code)
    {
    case GET_RESOURCES:
        paquetes_Packet *packet = encode_resources("CPU", "INTEL i10 9400", 60);
        soquetes_sendToClient(client_fd, packet, 1);
        break;
    default:
        printf("code not found");
        break;
    }

    soquetes_closeClientConn(client_fd, my_args->epoll_fd);
    printf("%s request handled and closed successfully\n", ip);

    return;
}
/**
 * the server keeps track of clients that have unexpected close their connections.
 * When that happens, we want to know
 */
void onConnectionClosed(int client_fd, void *args)
{
    char ip[INET6_ADDRSTRLEN];
    getClientIp(client_fd, ip);
    struct ConnectionArgs *my_args = args;
    soquetes_closeClientConn(client_fd, my_args->epoll_fd);
    printf("%s disconnected unexpectedly\n", ip);
}

/**
 * This is an example of a non-persisten server.
 *
 * i.e connections are closed after each request and response.
 * After the server sends the response to the client, the connection is terminated, and a new connection must be established for each subsequent request.
 */
int main()
{
    int server_fd = soquetes_createTcpServer(PORT, QUEUE_SIZE);
    if (server_fd == -1)
    {
        printf("err while creating server: [%s]\n", strerror(errno));
        return -1;
    }
    printf("Server started listening in port %s\n", PORT);

    /**
     * Here you can choose between the different server mechanism
     *
     * they will return only when the server crashes.
     */
    int epoll_fd;
    struct ConnectionArgs args;
    args.epoll_fd = epoll_fd;

    soquetes_AsyncServerConf conf;
    conf.server_fd = server_fd;
    conf.epoll_fd = &epoll_fd;
    conf.max_clients = MAX_CLIENTS;
    conf.args = &args;
    conf.requestHandler = handleRequest;
    conf.onConnectionClosed = onConnectionClosed;
    conf.onNewConnection = NULL;

    int status = soquetes_acceptConnectionsAsync(&conf);

    // soquetes_OnDemandServerConf conf;
    // conf.server_fd = server_fd;
    // conf.requestHandler = handleRequest;
    // conf.args = NULL;
    // conf.onConnectionClosed = onConnectionClosed;
    // conf.onNewConnection = NULL;
    // int status = soquetes_acceptConnectionsOnDemand(&conf);

    if (status == -1)
    {
        printf("the server has quit unexpectedly: %s\n", strerror(errno));
        return 1;
    }

    // should never reach this point
    return 0;
}