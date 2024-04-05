#ifndef PROTO_H
#define PROTO_H
#include "../../src/paquetes.h"

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

paquetes_Packet *encode_join(char *name)
{
    paquetes_Packet *packet = paquetes_create(JOIN);
    paquetes_addString(packet->buffer, name);

    return packet;
}

void decode_join(paquetes_Buffer *buffer, struct Join *join)
{
    join->name = paquetes_getString(buffer);
}

paquetes_Packet *encode_message(char *name, char *message)
{
    paquetes_Packet *packet = paquetes_create(NEW_MESSAGE);
    paquetes_addString(packet->buffer, name);
    paquetes_addString(packet->buffer, message);

    return packet;
}

void decode_message(paquetes_Buffer *buffer, struct Message *message)
{
    message->name = paquetes_getString(buffer);
    message->msg = paquetes_getString(buffer);
}

#endif