#ifndef PROTO_H
#define PROTO_H
#include "../../src/redilon.h"

enum ServerOperations
{
    PUBLISH_MESSAGE,
    JOIN
};

enum ClientOperations
{
    NEW_MESSAGE,
    JOIN_SUCCESS,
    JOIN_FAILURE
};

typedef struct Message
{
    char *name;
    char *msg;
} Message;

struct Join
{
    char *name;
};

redilon_Packet *encode_join(char *name)
{
    redilon_Packet *packet = redilon_createPacket(JOIN);
    redilon_addString(packet->buffer, name);

    return packet;
}

void decode_join(redilon_Buffer *buffer, struct Join *join)
{
    join->name = redilon_getString(buffer);
}

redilon_Packet *encode_message(char *name, char *message)
{
    redilon_Packet *packet = redilon_createPacket(NEW_MESSAGE);
    redilon_addString(packet->buffer, name);
    redilon_addString(packet->buffer, message);

    return packet;
}

void decode_message(redilon_Buffer *buffer, struct Message *message)
{
    message->name = redilon_getString(buffer);
    message->msg = redilon_getString(buffer);
}

#endif