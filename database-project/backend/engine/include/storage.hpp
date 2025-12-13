#pragma once
#include <string>
#include <nlohmann/json.hpp>

class Storage {
public:
    static void appendDocument(const std::string& filePath,
                               const nlohmann::json& doc);
};
