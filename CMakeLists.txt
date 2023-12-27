cmake_minimum_required(VERSION 3.5)

project(rf-gw LANGUAGES C)

execute_process(COMMAND
  git describe --always --dirty
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  OUTPUT_VARIABLE GIT_VERSION
  ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)

execute_process(COMMAND
  git describe --match=NeVeRmAtCh --always --abbrev=40 --dirty
  WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
  OUTPUT_VARIABLE GIT_SHA1
  ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)

add_definitions(-DCOMMIT_HASH="${GIT_SHA1}" -DCOMMIT_VERSION="${GIT_VERSION}" -DPROG_NAME="${PROJECT_NAME}")

include_directories(.)
include_directories(lib/ds/src)
include_directories(lib/jsmn)
include_directories(lib/modules)

list(APPEND LIB_DS lib/ds/src/hashmap.c lib/ds/src/iterator.c)
list(APPEND LIB_MODULES lib/modules/eventloop.c lib/modules/log.c lib/modules/signals.c lib/modules/timer.c)
list(APPEND LIB_MODULES lib/modules/tcpserver.c lib/modules/websocket.c)
list(APPEND LIB_MODULES lib/modules/rfserver.c)

add_definitions(-DLOG_DEFAULT=LOG_DEBUG)

add_executable(${PROJECT_NAME} crc.c main.c ${LIB_DS} ${LIB_MODULES})
target_link_libraries(${PROJECT_NAME} crypto)
target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Werror -pedantic -Wno-unused-result)
