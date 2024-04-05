#ifndef redilon_H
#define redilon_H

#include <stdlib.h>
#include <stdint.h>

// Structures
typedef struct Buffer
{
    uint32_t size;
    uint32_t offset;
    void *stream;
} redilon_Buffer;

typedef struct redilon_Packet
{
    uint8_t op_code;
    redilon_Buffer *buffer;

} redilon_Packet;

typedef void (*redilon_Handler)(uint8_t client_fd, uint8_t operation, redilon_Buffer *buffer, void *args);

typedef struct redilon_AsyncServerConf
{
    int server_fd;
    int *epoll_fd;
    int max_clients;
    /**
     * gets passed to all the handlers args (requestHandler, onConnectionClosed, onNewConnection).
     */
    void *handlersArgs;
    /**
     * gets fired when connected client sends data.
     */
    redilon_Handler requestHandler;
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
} redilon_AsyncServerConf;

typedef struct redilon_OnDemandServerConf
{
    int server_fd;
    void *args;
    redilon_Handler requestHandler;
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
} redilon_OnDemandServerConf;

// sockets
int redilon_read(int fd, redilon_Handler requestHandler, void *args);
// server
int redilon_createTcpServer(char *port, unsigned int queue_size);
int redilon_acceptConnectionsAsync(redilon_AsyncServerConf *conf);
int redilon_acceptConnectionsOnDemand(redilon_OnDemandServerConf *conf);
int redilon_sendToClient(int client_fd, redilon_Packet *packet, int should_free);
void redilon_closeClientConn(int client_fd, int epoll_fd);
// client
int redilon_connectToTcpServer(char *host, char *port);
int redilon_sendToServer(int server_fd, redilon_Packet *packet, redilon_Handler requestHandler, void *handler_args);
void redilon_closeServerConn(int server_fd);

// packets
redilon_Packet *redilon_createPacket(uint8_t op_code);
void *redilon_serializePacket(redilon_Packet *packet);
int redilon_getPacketSize(redilon_Packet *packet);
void redilon_freePacket(redilon_Packet *packet);
// add
int redilon_addUInt8(redilon_Buffer *buffer, uint8_t value);
int redilon_addUInt32(redilon_Buffer *buffer, uint32_t value);
int redilon_addUInt64(redilon_Buffer *buffer, uint64_t value);
int redilon_addString(redilon_Buffer *buffer, char *value);
// get
uint8_t redilon_getUInt8(redilon_Buffer *buffer);
uint32_t redilon_getUInt32(redilon_Buffer *buffer);
uint64_t redilon_getUInt64(redilon_Buffer *buffer);
char *redilon_getString(redilon_Buffer *buffer);

#endif // redilon_H
