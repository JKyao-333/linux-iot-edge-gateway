cmake_minimum_required(VERSION 3.16)

project(linux_iot_edge_gateway LANGUAGES CXX)
enable_testing()
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
find_package(yaml-cpp REQUIRED)
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
)

target_include_directories(app_core
    PUBLIC
        src
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
add_library(file_cache
    src/cache/file_cache.cpp
)

target_include_directories(file_cache
    PUBLIC
        src
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
        file_cache
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

target_link_libraries(edge_gateway
    PRIVATE
        protocol
	tcp_client
        app_core
	gateway_config
        mqtt_client
        logger
        reliable_publisher
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
)
add_executable(cache_replay_test
    tests/cache_replay_test.cpp
)

target_link_libraries(cache_replay_test
    PRIVATE
        reliable_publisher
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

set_tests_properties(
    gateway_config_invalid_test
    PROPERTIES
        LABELS "config"
        TIMEOUT 5
)
set_tests_properties(
    mqtt_client_test
    cache_replay_test
    PROPERTIES
        LABELS "mqtt"
        TIMEOUT 10
)
