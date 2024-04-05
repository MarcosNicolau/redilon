#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include "arpa/inet.h"
#include "./proto.h"
#include "../../src/redilon.h"

// just arbitrary values
#define QUEUE_SIZE 10
#define PORT "8000"
#define MAX_CLIENTS 100
#define ADMIN_NAME "ADMIN"

typedef struct Client
{
    int fd;
    char *ip;
    char *name;
} Client;

struct ConnectionArgs
{
    int epoll_fd;
    int clients_size;
    struct Client **clients;
};

char *concatenateStrings(char *str1, char *str2)
{
    size_t len1 = strlen(str1);
    size_t len2 = strlen(str2);

    char *result = malloc(len1 + len2 + 1);
    if (result == NULL)
        return NULL;

    strcpy(result, str1);

    strcat(result, str2);

    return result;
}

void getClientIp(int client_fd, char *ip)
{
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);

    if (getpeername(client_fd, (struct sockaddr *)&client_addr, &addr_len) == -1)
        return;
    inet_ntop(AF_INET, &(client_addr.sin_addr), ip, INET_ADDRSTRLEN);
}

Client *removeClient(int fd, struct Client **clients, int *clients_size)
{
    int idxToRmv = -1;

    // find index by comparing file descriptor
    for (int k = 0; k < *clients_size; ++k)
    {
        if (clients[k]->fd != fd)
            continue;
        idxToRmv = k;
        break;
    }

    if (idxToRmv == -1)
        return NULL;

    Client *client = clients[idxToRmv];

    // remove it
    for (int i = idxToRmv; i < *clients_size; i++)
        clients[i] = clients[i + 1];

    *clients_size -= 1;
    return client;
}

int addClient(int fd, char *ip, char *name, Client **clients, int *size)
{
    Client *client = malloc(sizeof(Client));
    if (client == NULL)
        return -1;
    client->fd = fd;
    client->ip = ip;
    client->name = name;
    clients[*size] = client;
    *size += 1;

    return 0;
}

void broadcastMessage(Message *message, Client **clients, int size)
{
    redilon_Packet *packet_message = encode_message(message->name, message->msg);
    if (packet_message == NULL)
        return;

    // send to all clients
    for (int i = 0; i < size; i++)
    {
        // but not to the sender
        if (!strcmp(clients[i]->name, message->name))
            continue;
        redilon_sendToClient(clients[i]->fd, packet_message, 0);
    }
    free(packet_message);
};

void handleRequest(uint8_t client_fd, uint8_t op_code, redilon_Buffer *buffer, void *args)
{
    char ip[INET6_ADDRSTRLEN];
    getClientIp(client_fd, ip);
    printf("Handling request with code %d from %s\n", op_code, ip);
    struct ConnectionArgs *my_args = args;

    switch (op_code)
    {
    case PUBLISH_MESSAGE:
        Message pubMsg;
        decode_message(buffer, &pubMsg);
        if (!strcmp(pubMsg.msg, ""))
            break;
        broadcastMessage(&pubMsg, my_args->clients, my_args->clients_size);
        printf("new message sent\n");
        break;
    case JOIN:
        struct Join join;
        decode_join(buffer, &join);

        // add client
        int added = addClient(client_fd, ip, join.name, my_args->clients, &my_args->clients_size);
        // ack
        redilon_Packet *packet_join = redilon_createPacket(added == -1 ? JOIN_FAILURE : JOIN_SUCCESS);
        redilon_sendToClient(client_fd, packet_join, 1);

        if (added == -1)
            break;

        // the client was added, tell the chat
        Message message;
        message.name = ADMIN_NAME;
        message.msg = concatenateStrings(join.name, " has just popped in");
        // the -1 is to not send it to the user that hast just joined
        broadcastMessage(&message, my_args->clients, my_args->clients_size - 1);
        printf("new peer joined\n");

        break;
    default:
        printf("code not found\n");
        break;
    }

    return;
}
/**
 * the server keeps track of clients that have unexpectedly close their connections.
 * When that happens, we remove the client from the chat.
 */
void onConnectionClosed(int client_fd, void *args)
{
    char ip[INET6_ADDRSTRLEN];
    getClientIp(client_fd, ip);
    struct ConnectionArgs *my_args = args;

    Client *client = removeClient(client_fd, my_args->clients, &my_args->clients_size);
    redilon_closeClientConn(client_fd, my_args->epoll_fd);
    printf("%s disconnected unexpectedly\n", ip);

    if (client == NULL)
        return;

    struct Message message;
    message.name = ADMIN_NAME;
    message.msg = concatenateStrings(client->name, " has left the chat");
    broadcastMessage(&message, my_args->clients, my_args->clients_size);

    free(client);
}

void onNewConnection(int client_fd, void *args)
{
    char ip[INET6_ADDRSTRLEN];
    getClientIp(client_fd, ip);
    printf("New client connected from %s\n", ip);
}

/**
 * This is an example of a chat server which keeps connections alive.
 *
 * i.e the connection between the client and the server is kept open after the initial request and response.
 * allowing subsequent requests and responses to be sent over the same connection without the overhead of establishing a new connection each time.
 */
int main()
{
    int server_fd = redilon_createTcpServer(PORT, QUEUE_SIZE);
    if (server_fd == -1)
    {
        printf("err while creating server: [%s]\n", strerror(errno));
        return -1;
    }
    printf("Server started listening in port %s\n", PORT);

    int epoll_fd;
    // let's pretend we support only 10 clients
    // in a real scenario this would be dynamically allocated
    struct Client **clients = malloc(sizeof(struct Client) * 10);

    struct ConnectionArgs args;
    args.epoll_fd = epoll_fd;
    args.clients_size = 0;
    args.clients = clients;

    redilon_AsyncServerConf conf;
    conf.server_fd = server_fd;
    conf.epoll_fd = &epoll_fd;
    conf.max_clients = MAX_CLIENTS;
    conf.handlersArgs = &args;
    conf.requestHandler = handleRequest;
    conf.onConnectionClosed = onConnectionClosed;
    conf.onNewConnection = onNewConnection;

    int status = redilon_acceptConnectionsAsync(&conf);

    if (status == -1)
    {
        printf("the server has quit unexpectedly: %s\n", strerror(errno));
        return 1;
    }

    // should never reach this point
    return 0;
}