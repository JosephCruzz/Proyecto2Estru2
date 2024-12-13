#ifndef SUPERBLOCK_H
#define SUPERBLOCK_H

#include <cstdint>

struct SuperBlock
{
 uint32_t blockSize;
 uint32_t blockCount;
 uint32_t inodeStart;  // Bloque inicial de inodos
 uint32_t inodeBlocks; // Cantidad de bloques de inodos
 uint32_t inodeCount;  // Cantidad total de inodos
 uint32_t dataStart;   // Bloque inicial de datos
};

#endif // SUPERBLOCK_H
