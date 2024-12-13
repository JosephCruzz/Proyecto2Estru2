cmake_minimum_required(VERSION 3.14)

# Nombre del proyecto
project(DispositivoDeBloques_JOSEPHCRUZ22151086)

# Establecer el estándar de C++
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Agregar los archivos fuente
set(SOURCES
    main.cpp
    BlockDevice.cpp
    FileSystem.cpp
)

# Crear el ejecutable
add_executable(${PROJECT_NAME} ${SOURCES})

# Incluir directorios para los encabezados (.h)
target_include_directories(${PROJECT_NAME}
    PRIVATE ${CMAKE_SOURCE_DIR}
)