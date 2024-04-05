#ifndef PROTO_H
#define PROTO_H
#include "../../src/paquetes.h"

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

paquetes_Packet *encode_resources(char *module, char *desc, uint32_t load)
{
    paquetes_Packet *packet = paquetes_create(SUCCESS);
    paquetes_addString(packet->buffer, module);
    paquetes_addString(packet->buffer, desc);
    paquetes_addUInt32(packet->buffer, load);

    return packet;
}

void decode_resources(paquetes_Buffer *buffer, struct Resources *resources)
{
    resources->module = paquetes_getString(buffer);
    resources->description = paquetes_getString(buffer);
    resources->load = paquetes_getUInt32(buffer);
}

#endif