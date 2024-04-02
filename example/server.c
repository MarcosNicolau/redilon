#include <stdlib.h>
#include <stdio.h>
#include "arpa/inet.h"
#include "redilon/soquetes.h"
#include "redilon/paquetes.h"
#include "./proto.h"

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

void handleRequest(uint8_t client_fd, uint8_t op_code, paquetes_Buffer *buffer, void *args)
{
    char ip[INET6_ADDRSTRLEN];
    getClientIp(client_fd, ip);
    printf("Client connected from %s\n", ip);

    switch (op_code)
    {
    case GET_RESOURCES:
        paquetes_Packet *packet = encode_resources("CPU", "INTEL i10 9400", 60);
        soquetes_sendToClient(client_fd, packet);
        break;
    default:
        printf("code not found");
        break;
    }

    return;
}

int main()
{
    int server_fd = soquetes_createTcpServer(PORT, QUEUE_SIZE);
    if (server_fd == -1)
    {
        printf("err while creating server: [%s]", strerror(errno));
        return -1;
    }

    /**
     * Here you can choose between the different server mechanism
     *
     * they will return only when the server crashes.
     */
    printf("Server started listening in port %s\n", PORT);
    int status = soquetes_acceptConnectionsAsync(server_fd, MAX_CLIENTS, handleRequest, NULL);
    // int status = soquetes_acceptConnectionsOnDemand(server_fd, handleRequest, NULL);
    if (status == -1)
    {
        printf("the server has quit unexpectedly: %s", strerror(errno));
        return 1;
    }

    // should never reach this point
    return 0;
}