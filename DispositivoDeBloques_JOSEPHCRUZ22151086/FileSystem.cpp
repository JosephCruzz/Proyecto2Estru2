#include "FileSystem.h"
#include <iostream>
#include <fstream>
#include <cstring>
#include <algorithm>
#include <cstdio>

FileSystem::FileSystem(BlockDevice &device) : device(device)
{
 inodesPerBlock = device.blockSize / 136; // Debe ser 7 con 1024 y 136
 // Suponiendo blockSize=1024, inodesPerBlock=7
 // Para 256 inodos -> 256/7=36.57 => 37 bloques
 blocksForInodes = 37;
 inodes.resize(256);
 freeBlockMap.resize(256, 0);
}

bool FileSystem::format()
{
 if (device.blockCount == 0 || device.blockSize == 0)
 {
  std::cerr << "El dispositivo no está inicializado.\n";
  return false;
 }

 // Definir layout:
 // Bloque 0: SuperBlock
 // Bloque 1: FreeBlockMap (256 bytes)
 // Bloques 2..(2+37-1)=2..38: Inodos
 // Bloque 39 en adelante: Datos

 superBlock.blockSize = (uint32_t)device.blockSize;
 superBlock.blockCount = (uint32_t)device.blockCount;
 superBlock.inodeStart = 2;
 superBlock.inodeBlocks = blocksForInodes;
 superBlock.inodeCount = (uint32_t)inodes.size();                       // 256
 superBlock.dataStart = superBlock.inodeStart + superBlock.inodeBlocks; // 2+37=39

 // Inicializar mapa de bloques libres
 std::fill(freeBlockMap.begin(), freeBlockMap.end(), 0);

 // Marcar bloques usados: superblock(0), freeBlockMap(1), inodos(2..38)
 // total 1 (sb) +1(map) +37(inodos) = 39 bloques usados
 for (uint32_t i = 0; i < superBlock.dataStart; i++)
 {
  uint32_t byteIndex = i / 8;
  uint8_t bitIndex = i % 8;
  freeBlockMap[byteIndex] |= (1 << bitIndex);
 }

 // Inicializar inodos y se asegura que esten parcados como libres para futuros archivos
 for (auto &inode : inodes)
 {
  inode.free = 1; // libre
  inode.fileSize = 0;
  std::memset(inode.fileName, 0, 64);
  std::fill(std::begin(inode.dataBlocks), std::end(inode.dataBlocks), 0);
  inode.crc = 0;
  std::memset(inode.reserved, 0, 28);
 }

 // Guardar superblock
 {
  std::vector<char> sbData(device.blockSize, 0);
  std::memcpy(sbData.data(), &superBlock, sizeof(SuperBlock));
  if (!device.writeBlock(0, sbData))
  {
   std::cerr << "Error escribiendo el SuperBlock.\n";
   return false;
  }
 }

 // Guardar mapa de bloques libres
 if (!saveFreeBlockMap())
  return false;

 // Guardar inodos
 if (!saveInodes())
  return false;

 return true;
}

bool FileSystem::load()
{
 if (device.blockCount == 0 || device.blockSize == 0)
 {
  // No abierto el dispositivo
  return false;
 }

 // Leer superblock
 {
  auto sbData = device.readBlock(0);
  if (sbData.size() < sizeof(SuperBlock))
  {
   std::cerr << "Error leyendo SuperBlock.\n";
   return false;
  }
  std::memcpy(&superBlock, sbData.data(), sizeof(SuperBlock));
 }

 inodes.resize(superBlock.inodeCount, Inode());

 if (!loadFreeBlockMap())
 {
  std::cerr << "Error leyendo mapa de bloques libres.\n";
  return false;
 }

 if (!loadInodes())
 {
  std::cerr << "Error leyendo inodos.\n";
  return false;
 }

 return true;
}

bool FileSystem::save()
{
 // Guardar superblock
 {
  std::vector<char> sbData(device.blockSize, 0);
  std::memcpy(sbData.data(), &superBlock, sizeof(SuperBlock));
  if (!device.writeBlock(0, sbData))
  {
   std::cerr << "Error escribiendo el SuperBlock.\n";
   return false;
  }
 }

 if (!saveFreeBlockMap())
  return false;
 if (!saveInodes())
  return false;

 return true;
}

bool FileSystem::ls()
{
 std::cout << "Archivos en el sistema:\n";
 for (auto &inode : inodes)
 {
  if (inode.free == 0)
  {
   std::cout << inode.fileName << " (" << inode.fileSize << " bytes)\n";
  }
 }
 return true;
}

bool FileSystem::cat(const std::string &filename)
{
 auto idx = findInodeByName(filename);
 if (!idx)
 {
  std::cerr << "Archivo no encontrado.\n";
  return false;
 }
 Inode &inode = inodes[*idx];

 // Leer datos
 std::size_t remaining = inode.fileSize;
 for (auto blockNum : inode.dataBlocks)
 {
  if (blockNum == 0)
   break;
  if (remaining == 0)
   break;

  auto data = device.readBlock(blockNum);
  std::size_t toPrint = std::min((std::size_t)device.blockSize, remaining);
  std::cout.write(data.data(), toPrint);
  remaining -= toPrint;
 }
 std::cout << "\n";
 return true;
}

bool FileSystem::writeFile(const std::string &filename, const std::string &data)
{
 auto idx = findInodeByName(filename);
 if (!idx)
 {
  // Crear nuevo archivo
  idx = findFreeInode();
  if (!idx)
  {
   std::cerr << "No hay espacio para nuevos archivos.\n";
   return false;
  }
  Inode &inode = inodes[*idx];
  inode.free = 0; // ocupado
  std::strncpy(inode.fileName, filename.c_str(), 63);
  inode.fileSize = 0;
  std::fill(std::begin(inode.dataBlocks), std::end(inode.dataBlocks), 0);
 }

 Inode &inode = inodes[*idx];
 // Escribir datos en bloques
 std::size_t offset = 0;
 std::size_t total = data.size();

 // Calcular cuántos bloques necesitamos
 std::size_t neededBlocks = (total + device.blockSize - 1) / device.blockSize;
 if (neededBlocks > 8)
 {
  std::cerr << "El archivo excede el límite de bloques (8).\n";
  return false;
 }

 for (std::size_t i = 0; i < neededBlocks; i++)
 {
  if (inode.dataBlocks[i] == 0)
  {
   auto blk = allocateBlock();
   if (!blk)
   {
    std::cerr << "No hay bloques libres.\n";
    return false;
   }
   inode.dataBlocks[i] = *blk;
  }

  std::size_t toWrite = std::min((std::size_t)device.blockSize, total - offset);
  std::vector<char> blockData(data.begin() + offset, data.begin() + offset + toWrite);
  if (!device.writeBlock(inode.dataBlocks[i], blockData))
  {
   std::cerr << "Error escribiendo datos.\n";
   return false;
  }
  offset += toWrite;
 }

 inode.fileSize = (uint32_t)total;
 return save();
}

bool FileSystem::hexdump(const std::string &filename)
{
 auto idx = findInodeByName(filename);
 if (!idx)
 {
  std::cerr << "Archivo no encontrado.\n";
  return false;
 }
 Inode &inode = inodes[*idx];

 std::size_t remaining = inode.fileSize;
 for (auto blockNum : inode.dataBlocks)
 {
  if (blockNum == 0)
   break;
  if (remaining == 0)
   break;
  auto data = device.readBlock(blockNum);
  std::size_t toPrint = std::min((std::size_t)device.blockSize, remaining);

  for (std::size_t i = 0; i < toPrint; i++)
  {
   printf("%02X ", (unsigned char)data[i]);
  }
  remaining -= toPrint;
 }
 printf("\n");
 return true;
}

bool FileSystem::copyOut(const std::string &fsFilename, const std::string &hostFilename)
{
 auto idx = findInodeByName(fsFilename);
 if (!idx)
 {
  std::cerr << "Archivo no encontrado.\n";
  return false;
 }
 Inode &inode = inodes[*idx];

 std::ofstream ofs(hostFilename, std::ios::binary);
 if (!ofs)
 {
  std::cerr << "Error abriendo archivo en host.\n";
  return false;
 }

 std::size_t remaining = inode.fileSize;
 for (auto blockNum : inode.dataBlocks)
 {
  if (blockNum == 0 || remaining == 0)
   break;
  auto data = device.readBlock(blockNum);
  std::size_t toWrite = std::min((std::size_t)device.blockSize, remaining);
  ofs.write(data.data(), toWrite);
  remaining -= toWrite;
 }

 ofs.close();
 std::cout << "Archivo copiado al host exitosamente.\n";
 return true;
}

bool FileSystem::copyIn(const std::string &hostFilename, const std::string &fsFilename)
{
 std::ifstream ifs(hostFilename, std::ios::binary);
 if (!ifs)
 {
  std::cerr << "Error abriendo archivo en host.\n";
  return false;
 }

 std::string data((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
 ifs.close();

 return writeFile(fsFilename, data);
}

bool FileSystem::rm(const std::string &filename)
{
 auto idx = findInodeByName(filename);
 if (!idx)
 {
  std::cerr << "Archivo no encontrado.\n";
  return false;
 }

 Inode &inode = inodes[*idx];

 // Liberar bloques de datos
 std::size_t remaining = inode.fileSize;
 for (auto blk : inode.dataBlocks)
 {
  if (blk == 0)
   break;
  freeBlock(blk);
 }

 // Resetear inodo
 inode.free = 1;
 inode.fileSize = 0;
 std::memset(inode.fileName, 0, 64);
 std::fill(std::begin(inode.dataBlocks), std::end(inode.dataBlocks), 0);

 save();
 std::cout << "Archivo eliminado.\n";
 return true;
}

std::optional<uint32_t> FileSystem::allocateBlock()
{
 for (uint32_t i = superBlock.dataStart; i < superBlock.blockCount; i++)
 {
  uint32_t byteIndex = i / 8;
  uint8_t bitIndex = i % 8;
  if ((freeBlockMap[byteIndex] & (1 << bitIndex)) == 0)
  {
   // Bloque libre
   freeBlockMap[byteIndex] |= (1 << bitIndex);
   saveFreeBlockMap();
   return i;
  }
 }
 return std::nullopt;
}

void FileSystem::freeBlock(uint32_t blockNumber)
{
 if (blockNumber < superBlock.blockCount)
 {
  uint32_t byteIndex = blockNumber / 8;
  uint8_t bitIndex = blockNumber % 8;
  freeBlockMap[byteIndex] &= ~(1 << bitIndex);
  saveFreeBlockMap();
 }
}

std::optional<uint32_t> FileSystem::findInodeByName(const std::string &filename)
{
 for (uint32_t i = 0; i < inodes.size(); i++)
 {
  if (inodes[i].free == 0 && std::strncmp(inodes[i].fileName, filename.c_str(), 64) == 0)
  {
   return i;
  }
 }
 return std::nullopt;
}

std::optional<uint32_t> FileSystem::findFreeInode()
{
 for (uint32_t i = 0; i < inodes.size(); i++)
 {
  if (inodes[i].free == 1)
  {
   return i;
  }
 }
 return std::nullopt;
}

bool FileSystem::loadFreeBlockMap()
{
 auto data = device.readBlock(FREEBLOCKMAP_BLOCK);
 if (data.size() != device.blockSize)
 {
  return false;
 }
 // Solo necesitamos los primeros 256 bytes
 std::memcpy(freeBlockMap.data(), data.data(), 256);
 return true;
}

bool FileSystem::saveFreeBlockMap()
{
 std::vector<char> data(device.blockSize, 0);
 std::memcpy(data.data(), freeBlockMap.data(), 256);
 return device.writeBlock(FREEBLOCKMAP_BLOCK, data);
}

bool FileSystem::loadInodes()
{
 // Leer todos los inodos desde los bloques 2..(2+37-1)
 uint32_t startBlock = superBlock.inodeStart;
 uint32_t endBlock = startBlock + superBlock.inodeBlocks;
 uint32_t inodesRead = 0;

 for (uint32_t blk = startBlock; blk < endBlock; blk++)
 {
  auto blockData = device.readBlock(blk);
  if (blockData.size() != device.blockSize)
  {
   return false;
  }

  // En cada bloque hay up to inodesPerBlock inodos
  for (uint32_t i = 0; i < inodesPerBlock && inodesRead < inodes.size(); i++)
  {
   std::memcpy(&inodes[inodesRead], blockData.data() + i * 136, 136);
   inodesRead++;
  }
 }
 return true;
}

bool FileSystem::saveInodes()
{
 uint32_t startBlock = superBlock.inodeStart;
 uint32_t endBlock = startBlock + superBlock.inodeBlocks;

 uint32_t inodesWritten = 0;

 for (uint32_t blk = startBlock; blk < endBlock; blk++)
 {
  std::vector<char> blockData(device.blockSize, 0);
  for (uint32_t i = 0; i < inodesPerBlock && inodesWritten < inodes.size(); i++)
  {
   std::memcpy(blockData.data() + i * 136, &inodes[inodesWritten], 136);
   inodesWritten++;
  }
  if (!device.writeBlock(blk, blockData))
  {
   return false;
  }
 }
 return true;
}

uint32_t FileSystem::inodeBlockIndex(uint32_t i)
{
 // i-th inode está en el bloque: inodeStart + (i/inodesPerBlock)
 return superBlock.inodeStart + (i / inodesPerBlock);
}

uint32_t FileSystem::inodeOffsetInBlock(uint32_t i)
{
 // offset del inodo en el bloque
 return (i % inodesPerBlock) * 136;
}
