#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#include "BlockDevice.h"
#include "SuperBlock.h"
#include "Inode.h"
#include <vector>
#include <string>
#include <optional>
#include <iostream>

class FileSystem
{
public:
 FileSystem(BlockDevice &device);
 bool format();
 bool load();
 bool save();

 // Comandos FS
 bool ls();
 bool cat(const std::string &filename);
 bool writeFile(const std::string &filename, const std::string &data);
 bool hexdump(const std::string &filename);
 bool copyOut(const std::string &fsFilename, const std::string &hostFilename);
 bool copyIn(const std::string &hostFilename, const std::string &fsFilename);
 bool rm(const std::string &filename);

 // Manejo directo del mapa
 std::optional<uint32_t> allocateBlock();
 void freeBlock(uint32_t blockNumber);

private:
 BlockDevice &device;
 SuperBlock superBlock;
 std::vector<Inode> inodes;
 std::vector<uint8_t> freeBlockMap; // 256 bytes => 2048 bits = 2048 bloques

 static constexpr uint32_t FREEBLOCKMAP_BLOCK = 1;
 // Inodos a partir del bloque 2
 // 37 bloques de inodos para 256 inodos a 7 inodos/bloque
 // inodos: 2..(2+37-1)=2..38
 // datos a partir de 39

 uint32_t inodesPerBlock;
 uint32_t blocksForInodes;

 std::optional<uint32_t> findInodeByName(const std::string &filename);
 std::optional<uint32_t> findFreeInode();
 bool loadFreeBlockMap();
 bool saveFreeBlockMap();
 bool loadInodes();
 bool saveInodes();

 uint32_t inodeBlockIndex(uint32_t i);
 uint32_t inodeOffsetInBlock(uint32_t i);
};

#endif // FILESYSTEM_H
