#ifndef PROTO_H
#define PROTO_H
#include "../../src/redilon.h"

enum Operations
{
    GET_RESOURCES,
};

enum Status
{
    SUCCESS,
    ERROR,
};

struct Resources
{
    char *module;
    char *description;
    uint32_t load;
};

redilon_Packet *encode_resources(char *module, char *desc, uint32_t load)
{
    redilon_Packet *packet = redilon_createPacket(SUCCESS);
    redilon_addString(packet->buffer, module);
    redilon_addString(packet->buffer, desc);
    redilon_addUInt32(packet->buffer, load);

    return packet;
}

void decode_resources(redilon_Buffer *buffer, struct Resources *resources)
{
    resources->module = redilon_getString(buffer);
    resources->description = redilon_getString(buffer);
    resources->load = redilon_getUInt32(buffer);
}

#endif