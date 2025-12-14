#pragma once
#include <string>
#include <nlohmann/json.hpp>
#include <vector>     

class DatabaseEngine {
public:
    using json = nlohmann::json;   
    static void init(const std::string& rootPath);

    //function to create a new database
    static void createDatabase(const std::string& userId,const std::string& dbName);

    //function to create a new collection in a database

    static void createCollection(const std::string& userId,const std::string& dbName,const std::string& collection);

    //function to check if a document matches a filter  
    bool match(json doc, json filter);


    static void insert(const std::string& userId,
                       const std::string& dbName,
                       const std::string& collection,
                       const nlohmann::json& doc);

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
    static void recoverWAL(const std::string& dbPath);

};
