#pragma once
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

using json = nlohmann::json;

struct QueryNode;

class DatabaseEngine {
public:
    static void init(const std::string& rootPath);
    static void ensureUserRoot(const std::string& userId);
    static void createDatabase(const std::string& userId, const std::string& dbName);
    static void createCollection(const std::string& userId, const std::string& dbName, const std::string& collection);
    static std::vector<std::string> listDatabases(const std::string& userId);
    static void insert(const std::string& userId, const std::string& dbName, const std::string& collection, const json& doc);
    static void insertVector(const std::string& userId, const std::string& dbName, const std::string& collection, const json& doc);
    static std::vector<json> find(const std::string& userId, const std::string& dbName, const std::string& collection, const json& filter);
    static std::vector<json> queryVector(const std::string& userId, const std::string& dbName, const std::string& collection, const json& query);
    static bool updateOne(const std::string& userId, const std::string& dbName, const std::string& collection, const json& filter, const json& update);
    static bool deleteOne(const std::string& userId, const std::string& dbName, const std::string& collection, const json& filter);
    static bool match(const nlohmann::json& doc,
                      const nlohmann::json& filter);
};
