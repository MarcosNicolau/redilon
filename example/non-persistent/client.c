#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include "./proto.h"
#include "../../src/paquetes.h"
#include "../../src/soquetes.h"

// settings host as null to use the loopback interface address
#define HOST NULL
#define PORT "8000"

void handleResourceResponse(uint8_t client_fd, uint8_t status, paquetes_Buffer *buffer, void *args)
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
        return NULL;
    paquetes_Packet *packet = paquetes_create(GET_RESOURCES);
    // we pass the resources as an argument to the handle response
    int res = soquetes_sendToServer(server_fd, packet, handleResourceResponse, resources);
    if (res == -1)
        return NULL;
    soquetes_closeServerConn(server_fd);
    return resources;
}

int main()
{
    struct Resources *resources = requestResources();
    if (resources == NULL)
    {
        printf("request error: [%s]\n", strerror(errno));
        return 1;
    }
    printf("module: %s\ndesc: %s \nload: [%d]\n", resources->module, resources->description, resources->load);
    return 0;
}