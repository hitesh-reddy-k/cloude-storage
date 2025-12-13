#pragma once
#include <string>

class WAL {
public:
    static void log(const std::string& walPath,
                    const std::string& record);
};
