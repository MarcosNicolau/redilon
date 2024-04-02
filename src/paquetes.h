#ifndef paquetes_H
#define paquetes_H

#include <stdint.h>
#include <stddef.h>

typedef struct Buffer
{
    uint32_t size;
    uint32_t offset;
    void *stream;
} paquetes_Buffer;

typedef struct paquetes_Packet
{
    uint8_t op_code;
    paquetes_Buffer *buffer;

} paquetes_Packet;

// Function declarations
paquetes_Packet *paquetes_create(uint8_t op_code);
void *paquetes_serialize(paquetes_Packet *packet);
int paquetes_getSize(paquetes_Packet *packet);
void paquetes_free(paquetes_Packet *packet);
int paquetes_reallocateBuffer(paquetes_Buffer *buffer, uint64_t size);
// packet add
int paquetes_addUInt8(paquetes_Buffer *buffer, uint8_t value);
int paquetes_addUInt32(paquetes_Buffer *buffer, uint32_t value);
int paquetes_addUInt64(paquetes_Buffer *buffer, uint64_t value);
int paquetes_addString(paquetes_Buffer *buffer, char *value);
// packet get
uint8_t paquetes_getUInt8(paquetes_Buffer *buffer);
uint32_t paquetes_getUInt32(paquetes_Buffer *buffer);
uint64_t paquetes_getUInt64(paquetes_Buffer *buffer);
char *paquetes_getString(paquetes_Buffer *buffer);

#endif /* paquetes_H */
