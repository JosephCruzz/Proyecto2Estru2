#include "BlockDevice.h"
#include <iostream>
#include <fstream>

bool BlockDevice::create(const std::string &filename, std::size_t bSize, std::size_t bCount)
{
 if (bSize == 0 || bCount == 0)
 {
  std::cerr << "El tamaño de bloque y la cantidad de bloques deben ser mayores a 0.\n";
  return false;
 }

 blockSize = bSize;
 blockCount = bCount;

 // Saca el tamaño total del archivo multiplicando el tamaño del bloque por su cantidad. tambien sumamos la metadata
 std::size_t file_size = metadata_size + (blockSize * blockCount);

 file.open(filename, std::ios::binary | std::ios::trunc | std::ios::out);
 if (!file.is_open())
 {
  std::cerr << "No se pudo crear el archivo.\n";
  return false;
 }

 // Escribir metadata inicial (blockSize, blockCount) se escribe al inicio del archivo y se guarda como metadata
 file.write(reinterpret_cast<const char *>(&blockSize), sizeof(blockSize));
 file.write(reinterpret_cast<const char *>(&blockCount), sizeof(blockCount));

 // Rellenar con ceros hasta el tamaño total se pone el puntero al tamaño del archivo y al final se escribe un byte vacio y lo demas estan llenos de ceros.
 file.seekp(file_size - 1, std::ios::beg);
 file.write("", 1);

 file.close();

 return true;
}

bool BlockDevice::open(const std::string &filename)
{
 file.open(filename, std::ios::binary | std::ios::in | std::ios::out);
 if (!file.is_open())
 {
  std::cerr << "El archivo no se pudo abrir.\n";
  return false;
 }

 // Saca la informacion del blocksize y el blockcount y lo pone en el

 file.read(reinterpret_cast<char *>(&blockSize), sizeof(blockSize));
 file.read(reinterpret_cast<char *>(&blockCount), sizeof(blockCount));

 if (!file)
 {
  std::cerr << "Error al leer la metadata.\n";
  file.close();
  return false;
 }

 std::cout << "El dispositivo se abrió exitosamente.\n";
 return true;
}

// funcion basica para cerrar un archivo
bool BlockDevice::close()
{
 if (file.is_open())
 {
  file.close();
  std::cout << "Dispositivo cerrado.\n";
  return true;
 }
 else
 {
  std::cerr << "No hay dispositivo abierto.\n";
  return false;
 }
}

bool BlockDevice::writeBlock(std::size_t blockNumber, const std::vector<char> &data)
{

 // Que si el numero del bloque es mas que la cantidad de bloque esta buscando un numero de bloque que no existe todavia normalmente porque es muy alto
 if (blockNumber >= blockCount)
 {
  std::cerr << "Número de bloque inválido.\n";
  return false;
 }
 // Que si la cantidad de char en este caso una palabra es mas grande que el tamaño del bloque obviamente no cabe en el bloque
 if (data.size() > blockSize)
 {
  std::cerr << "Datos demasiado grandes para el bloque.\n";
  return false;
 }

 std::size_t offset = metadata_size + (blockNumber * blockSize);
 // se pone el puntero al final del archivo
 file.seekp(offset, std::ios::beg);

 if (!file)
 {
  std::cerr << "Error al posicionar el apuntador de escritura.\n";
  return false;
 }

 // Escribir datos
 file.write(data.data(), data.size());

 // Si faltan bytes para completar el bloque, rellenar con ceros
 if (data.size() < blockSize)
 {
  std::vector<char> zeros(blockSize - data.size(), 0);
  // el tamaño del vector es exactamente lo que el falta para llenar el archivo
  file.write(zeros.data(), zeros.size());
 }

 if (!file)
 {
  std::cerr << "Error escribiendo en el bloque.\n";
  return false;
 }

 return true;
}

std::vector<char> BlockDevice::readBlock(std::size_t blockNumber)
{
 std::vector<char> vec;

 if (blockNumber >= blockCount)
 {
  std::cerr << "Número de bloque inválido.\n";
  return vec;
 }

 std::size_t offset = metadata_size + (blockNumber * blockSize);
 file.seekg(offset, std::ios::beg);
 if (!file)
 {
  std::cerr << "Error al posicionar el apuntador de lectura.\n";
  return vec;
 }

 vec.resize(blockSize, 0);
 file.read(vec.data(), blockSize);
 if (!file)
 {
  std::cerr << "Error leyendo el bloque.\n";
  vec.clear();
 }

 return vec;
}
