#pragma once
#include <string>
#include <nlohmann/json.hpp>
#include <vector>

class DatabaseEngine {
public:
    using json = nlohmann::json;

    static void init(const std::string& rootPath);

    static void createDatabase(
        const std::string& userId,
        const std::string& dbName
    );

    static void createCollection(
        const std::string& userId,
        const std::string& dbName,
        const std::string& collection
    );

    static void insert(
        const std::string& userId,
        const std::string& dbName,
        const std::string& collection,
        const json& doc
    );

    static std::vector<json> find(
        const std::string& userId,
        const std::string& dbName,
        const std::string& collection,
        const json& filter
    );

    static bool updateOne(
        const std::string& userId,
        const std::string& dbName,
        const std::string& collection,
        const json& filter,
        const json& update
    );

    static bool deleteOne(
        const std::string& userId,
        const std::string& dbName,
        const std::string& collection,
        const json& filter
    );

    static bool match(json doc, json filter);
};
