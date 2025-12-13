#pragma once
#include <string>
#include <nlohmann/json.hpp>

class DatabaseEngine {
public:

    static void init(const std::string& rootPath);
    static void createDatabase(const std::string& userId,
                               const std::string& dbName);

    static void createCollection(const std::string& userId,
                                 const std::string& dbName,
                                 const std::string& collection);

    static void insert(const std::string& userId,
                       const std::string& dbName,
                       const std::string& collection,
                       const nlohmann::json& doc);
};
