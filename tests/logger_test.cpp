#include "log/logger.h"

#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>

int main() {
    const std::string log_path =
        "logs/logger_test.log";

    std::remove(log_path.c_str());

    {
        logging::Logger logger(
            log_path,
            logging::Level::Debug
        );

        logger.debug(
            "protocol",
            "parser state changed"
        );

        logger.info(
            "serial",
            "serial port opened"
        );

        logger.warn(
            "sensor",
            "temperature out of range"
        );

        logger.error(
            "mqtt",
            "publish failed"
        );
    }

    std::ifstream input(log_path);
    std::string line;
    std::size_t line_count = 0;

    while (std::getline(input, line)) {
        ++line_count;
    }

    std::cout << "log line count: "
              << line_count << std::endl;

    return line_count == 4 ? 0 : 1;
}
