#ifndef INODE_H
#define INODE_H

#include <cstdint>

struct Inode
{
 char fileName[64];      // 64 bytes
 uint32_t fileSize;      // 4 bytes
 uint32_t dataBlocks[8]; // 8*4=32 bytes este contiene los indices
 uint8_t free;           // 1 byte (1=libre,0=ocupado)
 uint8_t padding[3];     // 3 bytes para alinear
 uint32_t crc;           // 4 bytes
 char reserved[28];      // relleno hasta 136 bytes este
 // ese reserved solo fue hecho para que el programa se mirara
 // profesional y realista
 // Total:64+4+32+1+3+4+28=136
};

#endif // INODE_H
