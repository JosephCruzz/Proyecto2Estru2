#ifndef BLOCKDEVICE_H
#define BLOCKDEVICE_H

#include <cstddef>
#include <fstream>
#include <string>
#include <vector>
#include <cstdint>

class BlockDevice
{
public:
 BlockDevice() : blockCount(0), blockSize(0) {}
 BlockDevice(std::size_t blockCount, std::size_t blockSize) : blockCount(blockCount), blockSize(blockSize) {}

 bool create(const std::string &filename, std::size_t block_size, std::size_t block_count);
 bool open(const std::string &filename);
 bool close();
 bool writeBlock(std::size_t blockNumber, const std::vector<char> &data);
 std::vector<char> readBlock(std::size_t blockNumber);

 std::size_t blockCount;
 std::size_t blockSize;

private:
 std::fstream file;
 static constexpr std::size_t metadata_size = 16; // 8 bytes para blockSize y blockCount + 8 relleno
 static constexpr std::size_t blockMetaSize = 4;
};

#endif // BLOCKDEVICE_H
