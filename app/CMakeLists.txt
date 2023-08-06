list(APPEND OVERLAY_CONFIG "thread-med.conf")
list(APPEND OVERLAY_CONFIG "secret.conf")
list(APPEND OVERLAY_CONFIG "dns.conf")
list(APPEND OVERLAY_CONFIG "mqtt.conf")

cmake_minimum_required(VERSION 3.20.0)
list(APPEND BOARD_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

execute_process(COMMAND git describe
                WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                OUTPUT_VARIABLE GIT_DESCRIBE
                ERROR_QUIET)
string(STRIP "${GIT_DESCRIBE}" GIT_DESCRIBE)
STRING(REGEX MATCH "[0-9]+\\.[0-9]+\\.[0-9]+"
       VERSION "${GIT_DESCRIBE}")

project(app LANGUAGES C VERSION ${VERSION})

configure_file(app_version.h.in ${CMAKE_BINARY_DIR}/app/include/app_version.h)
target_include_directories(app PRIVATE ${CMAKE_BINARY_DIR}/app/include src)

target_sources(app PRIVATE
       src/drv_i2c_master.c
       src/drv_digital_in.c
       src/drv_digital_out.c
       src/hvac.c
       src/temphum24.c
       src/openthread.c
       src/mqtt.c
       src/ha.c
       src/reset.c
       src/main.c)
