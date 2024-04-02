#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include "./proto.h"
#include "redilon/soquetes.h"
#include "redilon/paquetes.h"

// settings host as null to use the loopback interface address
#define HOST NULL
#define PORT "8000"

void handleResourceRes(uint8_t client_fd, uint8_t status, paquetes_Buffer *buffer, void *args)
{
    if (status == SUCCESS)
    {
        // we should receiving a pointer to a resources struct as an argument
        decode_resources(buffer, args);
    }
    else
        args = NULL;
}

struct Resources *requestResources()
{
    struct Resources *resources = malloc(sizeof(struct Resources));
    int server_fd = soquetes_connectToTcpServer(HOST, PORT);
    if (server_fd == -1)
    {
        printf("err while connecting to server: [%s]\n", strerror(errno));
        return NULL;
    }
    paquetes_Packet *packet = paquetes_create(GET_RESOURCES);
    // we pass the resources as an argument to the handle response
    soquetes_sendToServer(server_fd, packet, handleResourceRes, resources);
    return resources;
}

int main()
{
    struct Resources *resources = requestResources();
    printf("module: %s\ndesc: %s \nload: [%d]\n", resources->module, resources->description, resources->load);
    return 0;
}