#include "BlockDevice.h"
#include "FileSystem.h"
#include <iostream>
#include <sstream>
#include <vector>
#include <string>

static std::vector<std::string> splitInput(const std::string &input)
{
 std::istringstream iss(input);
 std::vector<std::string> tokens;
 std::string token;
 while (iss >> token)
 {
  tokens.push_back(token);
 }
 return tokens;
}

int main()
{
 BlockDevice *device = nullptr;
 FileSystem *fs = nullptr;
 std::string command;

 std::cout << "SISTEMA DE ARCHIVOS SIMPLE + DISPOSITIVO DE BLOQUES\n";
 std::cout << "Escribe 'help' ver los comandos disponibles.\n";

 while (true)
 {
  std::cout << "> ";
  if (!std::getline(std::cin, command))
   break;

  if (command.empty())
   continue;

  auto args = splitInput(command);

  if (args.empty())
   continue;

  if (args[0] == "help")
  {
   std::cout << "Comandos disponibles:\n";
   std::cout << "Parte 1 (Dispositivo Bloques):\n";
   std::cout << "  create <nombre> <tamaño_bloque> <cantidad_bloques>\n";
   std::cout << "  open <nombre>\n";
   std::cout << "  info\n";
   std::cout << "  dwrite <numero_bloque> <texto>\n";
   std::cout << "  dread <numero_bloque> <offset> <length>\n";
   std::cout << "  close\n";
   std::cout << "  exit\n\n";

   std::cout << "Parte 2 (Sistema de Archivos):\n";
   std::cout << "  format\n";
   std::cout << "  ls\n";
   std::cout << "  cat <archivo>\n";
   std::cout << "  write <archivo> <texto>\n";
   std::cout << "  hexdump <archivo>\n";
   std::cout << "  copy out <archivo_fs> <archivo_host>\n";
   std::cout << "  copy in <archivo_host> <archivo_fs>\n";
   std::cout << "  rm <archivo>\n";
  }
  else if (args[0] == "create" && args.size() == 4)
  {
   std::string filename = args[1];
   std::size_t blockSize = std::stoul(args[2]);
   std::size_t blockCount = std::stoul(args[3]);
   if (device)
    delete device;

   device = new BlockDevice(blockCount, blockSize);
   if (device->create(filename, blockSize, blockCount))
   {
    std::cout << "Dispositivo creado exitosamente.\n";
   }
   else
   {
    std::cerr << "Error al crear el dispositivo.\n";
    delete device;
    device = nullptr;
   }
  }
  else if (args[0] == "open" && args.size() == 2)
  {
   std::string filename = args[1];
   if (!device)
    device = new BlockDevice();
   if (device->open(filename))
   {
    if (fs)
     delete fs;
    fs = new FileSystem(*device);
    if (!fs->load())
    {
     std::cout << "El dispositivo no parece tener un FS formateado.\n";
    }
   }
   else
   {
    std::cerr << "Error al abrir el dispositivo.\n";
    if (device)
    {
     delete device;
     device = nullptr;
    }
   }
  }
  else if (args[0] == "info")
  {
   if (device && device->blockCount > 0 && device->blockSize > 0)
   {
    std::cout << "Información del dispositivo:\n";
    std::cout << "Tamaño de bloque: " << device->blockSize << " bytes\n";
    std::cout << "Cantidad de bloques: " << device->blockCount << "\n";
   }
   else
   {
    std::cerr << "No hay dispositivo abierto.\n";
   }
  }
  else if (args[0] == "dwrite" && args.size() >= 3)
  {
   if (!device)
   {
    std::cerr << "No hay dispositivo abierto.\n";
    continue;
   }
   std::size_t blockNumber = std::stoul(args[1]);
   // Tomar el resto del comando como data
   std::size_t pos = command.find(args[2]);
   if (pos == std::string::npos)
   {
    std::cerr << "Formato incorrecto.\n";
    continue;
   }
   std::string data = command.substr(pos);
   if (device->writeBlock(blockNumber, std::vector<char>(data.begin(), data.end())))
   {
    std::cout << "Escritura en el bloque " << blockNumber << " exitosa.\n";
   }
   else
   {
    std::cerr << "Error al escribir en el bloque.\n";
   }
  }
  else if (args[0] == "dread" && args.size() == 4)
  {
   if (!device)
   {
    std::cerr << "No hay dispositivo abierto.\n";
    continue;
   }
   std::size_t blockNumber = std::stoul(args[1]);
   std::size_t offset = std::stoul(args[2]);
   std::size_t length = std::stoul(args[3]);

   std::vector<char> data = device->readBlock(blockNumber);
   if (!data.empty())
   {
    if (offset + length <= data.size())
    {
     std::string output(data.begin() + offset, data.begin() + offset + length);
     std::cout << "Datos leídos: " << output << "\n";
    }
    else
    {
     std::cerr << "Offset+length exceden el tamaño de los datos.\n";
    }
   }
   else
   {
    std::cerr << "Error al leer el bloque o está vacío.\n";
   }
  }
  else if (args[0] == "close")
  {
   if (device && device->close())
   {
    std::cout << "Dispositivo cerrado exitosamente.\n";
    delete device;
    device = nullptr;
    if (fs)
    {
     delete fs;
     fs = nullptr;
    }
   }
   else
   {
    std::cerr << "No hay dispositivo abierto para cerrar.\n";
   }
  }
  else if (args[0] == "exit")
  {
   if (device)
   {
    device->close();
    delete device;
    device = nullptr;
   }
   if (fs)
   {
    delete fs;
    fs = nullptr;
   }
   break;
  }
  // FS Commands
  else if (args[0] == "format")
  {
   if (!device)
   {
    std::cerr << "No hay dispositivo abierto.\n";
    continue;
   }
   if (!fs)
    fs = new FileSystem(*device);
   if (fs->format())
   {
    std::cout << "Disco virtual formateado exitosamente.\n";
   }
   else
   {
    std::cerr << "Error al formatear.\n";
   }
  }
  else if (args[0] == "ls")
  {
   if (!fs)
   {
    std::cerr << "No hay FS cargado.\n";
    continue;
   }
   fs->ls();
  }
  else if (args[0] == "cat" && args.size() == 2)
  {
   if (!fs)
   {
    std::cerr << "No hay FS cargado.\n";
    continue;
   }
   fs->cat(args[1]);
  }
  else if (args[0] == "write" && args.size() >= 3)
  {
   if (!fs)
   {
    std::cerr << "No hay FS cargado.\n";
    continue;
   }
   std::string filename = args[1];
   std::size_t pos = command.find(filename);
   if (pos == std::string::npos)
   {
    std::cerr << "Formato incorrecto.\n";
    continue;
   }
   pos += filename.size();
   std::string data = command.substr(pos);
   data.erase(0, data.find_first_not_of(" "));
   if (fs->writeFile(filename, data))
   {
    std::cout << "Datos escritos en " << filename << " exitosamente.\n";
   }
   else
   {
    std::cerr << "Error al escribir el archivo.\n";
   }
  }
  else if (args[0] == "hexdump" && args.size() == 2)
  {
   if (!fs)
   {
    std::cerr << "No hay FS cargado.\n";
    continue;
   }
   fs->hexdump(args[1]);
  }
  else if (args[0] == "copy" && args.size() == 4 && args[1] == "out")
  {
   if (!fs)
   {
    std::cerr << "No hay FS cargado.\n";
    continue;
   }
   fs->copyOut(args[2], args[3]);
  }
  else if (args[0] == "copy" && args.size() == 4 && args[1] == "in")
  {
   if (!fs)
   {
    std::cerr << "No hay FS cargado.\n";
    continue;
   }
   fs->copyIn(args[2], args[3]);
  }
  else if (args[0] == "rm" && args.size() == 2)
  {
   if (!fs)
   {
    std::cerr << "No hay FS cargado.\n";
    continue;
   }
   fs->rm(args[1]);
  }
  else
  {
   std::cerr << "Comando no reconocido. 'help' para ayuda.\n";
  }
 }

 return 0;
}
