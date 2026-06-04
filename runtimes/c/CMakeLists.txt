cmake_minimum_required(VERSION 3.16)
project(ads_runtime_c C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

add_compile_options(-Wall -Wextra -Wpedantic -Werror=implicit-function-declaration)

# Optional sanitizer build: cmake -DCMAKE_BUILD_TYPE=Asan
set(CMAKE_C_FLAGS_ASAN "-O0 -g -fsanitize=address,undefined -fno-omit-frame-pointer")
set(CMAKE_LINKER_FLAGS_ASAN "-fsanitize=address,undefined")

find_package(ZLIB)
find_library(CJSON_LIB cjson)
find_path(CJSON_INCLUDE_DIR cjson/cJSON.h)

add_library(ads_runtime_c STATIC
    src/plugin.c
    src/helpers.c
    src/coordinate.c
    src/ascii85.c
    src/crc.c
    src/result_formatter.c
    src/string_list.c
    src/regex.c
)
target_include_directories(ads_runtime_c PUBLIC include)
target_include_directories(ads_runtime_c PRIVATE ${CJSON_INCLUDE_DIR})
target_link_libraries(ads_runtime_c PUBLIC ${CJSON_LIB})
if(ZLIB_FOUND)
    target_link_libraries(ads_runtime_c PUBLIC ZLIB::ZLIB)
    target_compile_definitions(ads_runtime_c PRIVATE ADS_HAVE_ZLIB)
endif()

# Optionally build a shared library too.
option(ADS_RUNTIME_SHARED "Also build a shared library variant" OFF)
if(ADS_RUNTIME_SHARED)
    add_library(ads_runtime_c_shared SHARED ${ADS_RUNTIME_SOURCES})
    set_target_properties(ads_runtime_c_shared PROPERTIES OUTPUT_NAME ads_runtime_c)
endif()

enable_testing()
