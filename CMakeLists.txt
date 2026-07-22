cmake_minimum_required(VERSION 3.16)

file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION"
    GATEWAY_VERSION
    LIMIT_COUNT 1
)
string(STRIP "${GATEWAY_VERSION}" GATEWAY_VERSION)

if(NOT GATEWAY_VERSION MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+$")
    message(FATAL_ERROR
        "VERSION must use MAJOR.MINOR.PATCH: ${GATEWAY_VERSION}"
    )
endif()

project(
    linux_iot_edge_gateway
    VERSION "${GATEWAY_VERSION}"
    LANGUAGES CXX
)

configure_file(
    src/version.h.in
    generated/version.h
    @ONLY
)

enable_testing()
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
find_package(yaml-cpp REQUIRED)
find_package(SQLite3 REQUIRED)
find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)

pkg_check_modules(
    MOSQUITTO
    REQUIRED
    IMPORTED_TARGET
    libmosquitto
)
add_library(protocol
    src/protocol/crc16.cpp
    src/protocol/frame_parser.cpp
)

target_include_directories(protocol
    PUBLIC
        src
)

add_library(app_core
    src/app/sensor_data.cpp
    src/device/device_data.cpp
)

target_include_directories(app_core
    PUBLIC
        src
)

add_library(runtime_control
    src/app/shutdown_signal.cpp
)

target_include_directories(runtime_control
    PUBLIC
        src
)

add_library(runtime_observability
    src/app/runtime_metrics.cpp
    src/app/runtime_status.cpp
)

target_include_directories(runtime_observability
    PUBLIC
        src
)

add_library(http_server
    src/http/http_server.cpp
)

target_include_directories(http_server
    PUBLIC
        src
)

target_link_libraries(http_server
    PUBLIC
        runtime_observability
        logger
        Threads::Threads
)

add_library(gateway_config
    src/config/gateway_config.cpp
)

target_include_directories(gateway_config
    PUBLIC
        src
)

target_link_libraries(gateway_config
    PUBLIC
        yaml-cpp
)
add_library(mqtt_client
    src/mqtt/mqtt_client.cpp
)

target_include_directories(mqtt_client
    PUBLIC
        src
)
target_link_libraries(mqtt_client
    PUBLIC
        PkgConfig::MOSQUITTO
)


add_library(tcp_client
    src/tcp/tcp_client.cpp
)

target_include_directories(tcp_client
    PUBLIC
        src

)
add_executable(tcp_client_test
    tests/tcp_client_test.cpp
)

target_link_libraries(tcp_client_test
    PRIVATE
        tcp_client
)

target_link_libraries(app_core
    PUBLIC
        protocol
)

add_library(modbus_protocol
    src/modbus/modbus_rtu.cpp
)

target_include_directories(modbus_protocol
    PUBLIC
        src
)

target_link_libraries(modbus_protocol
    PUBLIC
        protocol
        app_core
)

add_library(can_protocol
    src/can/can_parser.cpp
)

target_include_directories(can_protocol
    PUBLIC
        src
)

target_link_libraries(can_protocol
    PUBLIC
        app_core
)

add_library(device_input
    src/device/serial_port.cpp
    src/device/uart_device.cpp
    src/device/modbus_device.cpp
    src/device/can_device.cpp
    src/device/device_manager.cpp
    src/can/socketcan_device.cpp
)

target_include_directories(device_input
    PUBLIC
        src
)

target_link_libraries(device_input
    PUBLIC
        app_core
        protocol
        modbus_protocol
        can_protocol
        Threads::Threads
)

add_library(message_cache INTERFACE)

target_include_directories(message_cache
    INTERFACE
        src
)

add_library(file_cache
    src/cache/file_cache.cpp
)

target_include_directories(file_cache
    PUBLIC
        src
)

target_link_libraries(file_cache
    PUBLIC
        message_cache
)

add_library(sqlite_cache
    src/cache/sqlite_cache.cpp
)

target_include_directories(sqlite_cache
    PUBLIC
        src
)

target_link_libraries(sqlite_cache
    PUBLIC
        message_cache
        SQLite::SQLite3
)

add_library(reliable_publisher
    src/mqtt/reliable_publisher.cpp
)

target_include_directories(reliable_publisher
    PUBLIC
        src
)

target_link_libraries(reliable_publisher
    PUBLIC
        mqtt_client
        message_cache
        runtime_observability
)

add_library(publisher_group
    src/publisher/publisher_group.cpp
    src/publisher/tcp_publisher.cpp
)

target_include_directories(publisher_group
    PUBLIC
        src
)

target_link_libraries(publisher_group
    PUBLIC
        tcp_client
)

add_library(logger
    src/log/logger.cpp
)

target_include_directories(logger
    PUBLIC
        src
)
add_executable(edge_gateway
    src/main.cpp
)

target_include_directories(edge_gateway
    PRIVATE
        "${CMAKE_CURRENT_BINARY_DIR}/generated"
)

target_link_libraries(edge_gateway
    PRIVATE
        protocol
        tcp_client
        app_core
        runtime_control
        runtime_observability
        http_server
        device_input
        gateway_config
        mqtt_client
        logger
        reliable_publisher
        publisher_group
        file_cache
        sqlite_cache
        Threads::Threads
)
add_executable(mqtt_client_test
    tests/mqtt_client_test.cpp
)

target_link_libraries(mqtt_client_test
    PRIVATE
        mqtt_client
)
add_executable(file_cache_test
    tests/file_cache_test.cpp
)

target_link_libraries(file_cache_test
    PRIVATE
        file_cache
)
add_executable(file_cache_replace_test
    tests/file_cache_replace_test.cpp
)

target_link_libraries(file_cache_replace_test
    PRIVATE
        file_cache
)
add_executable(reliable_publisher_test
    tests/reliable_publisher_test.cpp
)

target_link_libraries(reliable_publisher_test
    PRIVATE
        reliable_publisher
        file_cache
)
add_executable(cache_replay_test
    tests/cache_replay_test.cpp
)

target_link_libraries(cache_replay_test
    PRIVATE
        reliable_publisher
        sqlite_cache
)

add_executable(logger_test
    tests/logger_test.cpp
)

target_link_libraries(logger_test
    PRIVATE
        logger
)

add_executable(frame_parser_crc_diagnostic_test
    tests/frame_parser_crc_diagnostic_test.cpp
)

target_link_libraries(frame_parser_crc_diagnostic_test
    PRIVATE
        protocol
)
add_executable(frame_parser_length_diagnostic_test
    tests/frame_parser_length_diagnostic_test.cpp
)

target_link_libraries(frame_parser_length_diagnostic_test
    PRIVATE
        protocol
)
add_executable(crc16_test
    tests/crc16_test.cpp
)

target_link_libraries(crc16_test
    PRIVATE
        protocol
)

add_test(
    NAME crc16_test
    COMMAND crc16_test
)
add_executable(frame_test
    tests/frame_test.cpp
)

target_link_libraries(frame_test
    PRIVATE
        protocol
)

add_test(
    NAME frame_test
    COMMAND frame_test
)
add_executable(frame_parser_test
    tests/frame_parser_test.cpp
)

target_link_libraries(frame_parser_test
    PRIVATE
        protocol
)

add_test(
    NAME frame_parser_test
    COMMAND frame_parser_test
)
add_executable(frame_parser_stream_test
    tests/frame_parser_stream_test.cpp
)

target_link_libraries(frame_parser_stream_test
    PRIVATE
        protocol
)

add_test(
    NAME frame_parser_stream_test
    COMMAND frame_parser_stream_test
)
add_executable(frame_parser_half_test
    tests/frame_parser_half_test.cpp
)

target_link_libraries(frame_parser_half_test
    PRIVATE
        protocol
)

add_test(
    NAME frame_parser_half_test
    COMMAND frame_parser_half_test
)
add_executable(frame_parser_bad_crc_test
    tests/frame_parser_bad_crc_test.cpp
)

target_link_libraries(frame_parser_bad_crc_test
    PRIVATE
        protocol
)

add_test(
    NAME frame_parser_bad_crc_test
    COMMAND frame_parser_bad_crc_test
)
add_test(
    NAME frame_parser_crc_diagnostic_test
    COMMAND frame_parser_crc_diagnostic_test
)
add_test(
    NAME frame_parser_length_diagnostic_test
    COMMAND frame_parser_length_diagnostic_test
)
function(register_gateway_test target source)
    if(NOT TARGET ${target})
        add_executable(${target}
            ${source}
        )

        target_link_libraries(${target}
            PRIVATE
                ${ARGN}
        )
    endif()

    if(NOT TEST ${target})
        add_test(
            NAME ${target}
            COMMAND ${target}
        )
    endif()
endfunction()

register_gateway_test(
    crc16_test
    tests/crc16_test.cpp
    protocol
)

register_gateway_test(
    frame_test
    tests/frame_test.cpp
    protocol
)

register_gateway_test(
    frame_parser_test
    tests/frame_parser_test.cpp
    protocol
)

register_gateway_test(
    frame_parser_stream_test
    tests/frame_parser_stream_test.cpp
    protocol
)

register_gateway_test(
    frame_parser_half_test
    tests/frame_parser_half_test.cpp
    protocol
)

register_gateway_test(
    frame_parser_bad_crc_test
    tests/frame_parser_bad_crc_test.cpp
    protocol
)

register_gateway_test(
    frame_parser_crc_diagnostic_test
    tests/frame_parser_crc_diagnostic_test.cpp
    protocol
)

register_gateway_test(
    frame_parser_length_diagnostic_test
    tests/frame_parser_length_diagnostic_test.cpp
    protocol
)

register_gateway_test(
    sensor_data_struct_test
    tests/sensor_data_struct_test.cpp
    app_core
)

register_gateway_test(
    sensor_data_parse_test
    tests/sensor_data_parse_test.cpp
    app_core
)

register_gateway_test(
    sensor_data_validate_test
    tests/sensor_data_validate_test.cpp
    app_core
)

register_gateway_test(
    sensor_data_json_test
    tests/sensor_data_json_test.cpp
    app_core
)

register_gateway_test(
    file_cache_test
    tests/file_cache_test.cpp
    file_cache
)

register_gateway_test(
    file_cache_replace_test
    tests/file_cache_replace_test.cpp
    file_cache
)

register_gateway_test(
    reliable_publisher_test
    tests/reliable_publisher_test.cpp
    reliable_publisher
)

register_gateway_test(
    publisher_group_test
    tests/publisher_group_test.cpp
    publisher_group
)

register_gateway_test(
    logger_test
    tests/logger_test.cpp
    logger
)

register_gateway_test(
    mqtt_client_test
    tests/mqtt_client_test.cpp
    mqtt_client
)

register_gateway_test(
    cache_replay_test
    tests/cache_replay_test.cpp
    reliable_publisher
)
register_gateway_test(
    gateway_config_test
    tests/gateway_config_test.cpp
    gateway_config
)

set_tests_properties(
    gateway_config_test
    PROPERTIES
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        LABELS "config"
        TIMEOUT 5
)

register_gateway_test(
    gateway_config_invalid_test
    tests/gateway_config_invalid_test.cpp
    gateway_config
)

register_gateway_test(
    runtime_observability_test
    tests/runtime_observability_test.cpp
    runtime_observability
)

register_gateway_test(
    http_server_test
    tests/http_server_test.cpp
    http_server
)

set_tests_properties(
    runtime_observability_test
    http_server_test
    PROPERTIES
        LABELS "observability"
        TIMEOUT 10
)

set_tests_properties(
    gateway_config_invalid_test
    PROPERTIES
        LABELS "config"
        TIMEOUT 5
)

register_gateway_test(
    shutdown_signal_test
    tests/shutdown_signal_test.cpp
    runtime_control
)

register_gateway_test(
    sqlite_cache_test
    tests/sqlite_cache_test.cpp
    sqlite_cache
)

register_gateway_test(
    sqlite_cache_persistence_test
    tests/sqlite_cache_persistence_test.cpp
    sqlite_cache
)

register_gateway_test(
    byte_ring_buffer_test
    tests/byte_ring_buffer_test.cpp
    protocol
)

register_gateway_test(
    frame_parser_ring_buffer_test
    tests/frame_parser_ring_buffer_test.cpp
    protocol
)

register_gateway_test(
    modbus_rtu_test
    tests/modbus/modbus_rtu_test.cpp
    modbus_protocol
)

register_gateway_test(
    can_parser_test
    tests/can/can_parser_test.cpp
    can_protocol
)

register_gateway_test(
    modbus_device_timeout_test
    tests/modbus/modbus_device_timeout_test.cpp
    device_input
)

register_gateway_test(
    device_manager_test
    tests/device_manager_test.cpp
    device_input
)

set_tests_properties(
    modbus_rtu_test
    can_parser_test
    modbus_device_timeout_test
    device_manager_test
    PROPERTIES
        LABELS "device;protocol"
        TIMEOUT 5
)

add_test(
    NAME release_version_test
    COMMAND bash
        "${CMAKE_SOURCE_DIR}/scripts/verify_release_version.sh"
        "v${PROJECT_VERSION}"
)

add_test(
    NAME version_cli_test
    COMMAND $<TARGET_FILE:edge_gateway> --version
)

set_tests_properties(
    release_version_test
    version_cli_test
    PROPERTIES
        LABELS "release"
        PASS_REGULAR_EXPRESSION "${PROJECT_VERSION}"
        TIMEOUT 5
)

set_tests_properties(
    sqlite_cache_test
    sqlite_cache_persistence_test
    PROPERTIES
        LABELS "cache;sqlite"
        TIMEOUT 5
)

set_tests_properties(
    shutdown_signal_test
    PROPERTIES
        LABELS "runtime"
        TIMEOUT 5
)

set_tests_properties(
    mqtt_client_test
    cache_replay_test
    PROPERTIES
        LABELS "mqtt"
        TIMEOUT 10
)
