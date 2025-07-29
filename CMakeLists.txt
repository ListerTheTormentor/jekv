cmake_minimum_required(VERSION 3.10)

project(test)

include_directories(include
    porting
    src
    third
)

list(APPEND JEKV_SRCS
    porting/jekv_porting_pc.c
    porting/jekv_log.c
    easy/jekv_easy.c
    src/jekv_base.c
    src/jekv_debug.c
    src/jekv_handler.c
    src/jekv_hash.c
    src/jekv_item.c
    src/jekv_iterator.c
    src/jekv_partition_manager.c
    src/jekv_partition.c
    src/jekv_sector_manager.c
    src/jekv_sector.c
    src/jekv_storage.c
)

add_executable(test
    ${JEKV_SRCS}
    example/main.c
)
