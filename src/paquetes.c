#include "stdlib.h"
#include "stdio.h"
#include "stdint.h"
#include "stddef.h"
#include "string.h"
#include "./paquetes.h"

/**
 * Returns the data structure to which you would data.
 * When you are ready to send it, you should serialize with `paquetes_serialize`
 *
 * @code
 * paquetes_t *packet = packet_create(<OP_CODE>);
 * paquetes_addUInt8(packet->buffer, 10);
 * paquetes_addString(packet->buffer, "Hello world1");
 * send(fd, paquetes_serialize(packet), pack_getSize(packet), 0);
 * paquetes_free(packet);
 *
 *
 * // later in your client you would
 * paquetes_t *packet = packet_create(0);
 * recv(fd, &(packet->op_code), sizeof(uint8_t), 0);
 * recv(fd, &(packet->buffer->size), sizeof(uint32_t), 0);
 * packet->buffer->stream = malloc(packet->buffer->size)
 * recv(fd, &(packet->buffer->stream), packet->buffer->size, 0);
 *
 * // and now we can start to read the data
 * int num = paquetes_getUInt8(packet->buffer);
 * int str = paquetes_getString(packet->buffer);
 */

paquetes_Packet *paquetes_create(uint8_t op_code)
{
    paquetes_Packet *packet = malloc(sizeof(paquetes_Packet));
    if (packet == NULL)
        return NULL;
    packet->buffer = malloc(sizeof(paquetes_Buffer));
    if (packet->buffer == NULL)
    {
        free(packet);
        return NULL;
    }
    packet->op_code = op_code;
    packet->buffer->stream = malloc(0);
    packet->buffer->offset = 0;
    packet->buffer->size = 0;
    return packet;
}

/**
 * @returns packet total size
 */
int paquetes_getSize(paquetes_Packet *packet)
{
    // sum of the op_code + buffer size field + buffer stream
    return sizeof(uint8_t) + sizeof(uint32_t) + packet->buffer->size;
};

/**
 * Serializes a packet.
 *
 * @returns Serialized packet on success, `NULL` on error
 */
void *paquetes_serialize(paquetes_Packet *packet)
{
    size_t size = paquetes_getSize(packet);
    void *serializedPacket = malloc(size);
    if (serializedPacket == NULL)
        return NULL;
    int offset = 0;
    memcpy(serializedPacket + offset, &(packet->op_code), sizeof(uint8_t));
    offset += sizeof(uint8_t);
    memcpy(serializedPacket + offset, &(packet->buffer->size), sizeof(uint32_t));
    offset += sizeof(uint32_t);
    memcpy(serializedPacket + offset, packet->buffer->stream, packet->buffer->size);

    return serializedPacket;
}

/**
 * Deallocates packet memory.
 */
void paquetes_free(paquetes_Packet *packet)
{
    free(packet->buffer->stream);
    free(packet->buffer);
    free(packet);
};

/**
 * Reallocates memory for the buffer.
 *
 * @returns 0 on success, -1 on error
 */
int paquetes_reallocateBuffer(paquetes_Buffer *buffer, size_t size)
{
    void *temp = realloc(buffer->stream, buffer->size + size);
    if (temp == NULL)
        return -1;
    buffer->stream = temp;
    buffer->size += size;
    return 0;
}

// packet add

/**
 * Adds a uint8_t value to the packet buffer.
 *
 * @returns 0 on success, -1 on error
 */
int paquetes_addUInt8(paquetes_Buffer *buffer, uint8_t value)
{
    if (paquetes_reallocateBuffer(buffer, sizeof(uint8_t)) == -1)
        return -1;
    memcpy(buffer->stream + buffer->offset, &value, sizeof(uint8_t));
    buffer->offset += sizeof(uint8_t);
    return 0; // Returns 0 on success
}

/**
 * Adds a uint32_t value to the packet buffer.
 *
 * @returns 0 on success, -1 on error
 */
int paquetes_addUInt32(paquetes_Buffer *buffer, uint32_t value)
{
    if (paquetes_reallocateBuffer(buffer, sizeof(uint32_t)) == -1)
        return -1;
    memcpy(buffer->stream + buffer->offset, &value, sizeof(uint32_t));
    buffer->offset += sizeof(uint32_t);
    return 0;
}

/**
 * Adds a uint64_t value to the packet buffer.
 *
 * @returns 0 on success, -1 on error
 */
int paquetes_addUInt64(paquetes_Buffer *buffer, uint64_t value)
{
    if (paquetes_reallocateBuffer(buffer, sizeof(uint64_t)) == -1)
        return -1;

    memcpy(buffer->stream + buffer->offset, &value, sizeof(uint64_t));
    buffer->offset += sizeof(uint64_t);
    return 0;
}

/**
 * Adds a string to the packet buffer.
 *
 * @returns 0 on success, -1 on error
 */
int paquetes_addString(paquetes_Buffer *buffer, char *value)
{
    uint32_t length = strlen(value) + 1;
    if (paquetes_addUInt32(buffer, length) == -1)
        return -1;
    if (paquetes_reallocateBuffer(buffer, length) == -1)
        return -1;

    memcpy(buffer->stream + buffer->offset, value, length);
    buffer->offset += length;
    return 0;
}

// packet get

/**
 * Reads a uint8_t value from the packet buffer.
 */
uint8_t paquetes_getUInt8(paquetes_Buffer *buffer)
{
    uint8_t value;
    memcpy(&value, buffer->stream + buffer->offset, sizeof(uint8_t));
    buffer->offset += sizeof(uint8_t);
    return value;
};

/**
 * Reads a uint32_t value from the packet buffer.
 */
uint32_t paquetes_getUInt32(paquetes_Buffer *buffer)
{
    uint32_t value;
    memcpy(&value, buffer->stream + buffer->offset, sizeof(uint32_t));
    buffer->offset += sizeof(uint32_t);
    return value;
};

/**
 * Reads a uint64_t value from the packet buffer.
 */
uint64_t paquetes_getUInt64(paquetes_Buffer *buffer)
{
    uint64_t value;
    memcpy(&value, buffer->stream + buffer->offset, sizeof(uint64_t));
    buffer->offset += sizeof(uint64_t);
    return value;
};

/**
 * Reads a string from the packet buffer.
 *
 * @returns Pointer to the string on success, `NULL` on error
 */

char *paquetes_getString(paquetes_Buffer *buffer)
{
    // we expect the string to have the length before the actual string
    uint32_t length = paquetes_getUInt32(buffer);
    char *str = malloc(length);
    if (str == NULL)
        return NULL;
    memcpy(str, buffer->stream + buffer->offset, length);
    buffer->offset += length;
    return str;
}
