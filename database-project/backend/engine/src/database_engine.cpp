#include "database_engine.hpp"
#include "storage.hpp"
#include "wal.hpp"
#include "logger.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include "query.hpp"


namespace fs = std::filesystem;
using json = nlohmann::json;

static std::string DATA_ROOT;

void DatabaseEngine::init(const std::string& rootPath) {
    DATA_ROOT = rootPath;
    if (DATA_ROOT.back() != '/' && DATA_ROOT.back() != '\\')
        DATA_ROOT += "/";

    fs::create_directories(DATA_ROOT);

    std::cout << "[ENGINE] Data root initialized:\n  "
              << fs::absolute(DATA_ROOT) << std::endl;
}

static fs::path basePath(const std::string& userId,
                         const std::string& dbName) {
    return fs::path(DATA_ROOT) / userId / dbName;
}

/* ---------------- DATABASE ---------------- */

void DatabaseEngine::createDatabase(const std::string& userId,
                                    const std::string& dbName) {
    fs::path base = basePath(userId, dbName);

    std::cout << "[ENGINE] Creating database: "
              << fs::absolute(base) << std::endl;

    fs::create_directories(base / "data");
    fs::create_directories(base / "wal");
    fs::create_directories(base / "logs");

    Logger::write((base / "logs/db.log").string(), "DATABASE CREATED");
}

/* ---------------- COLLECTION ---------------- */

void DatabaseEngine::createCollection(const std::string& userId,
                                      const std::string& dbName,
                                      const std::string& collection) {
    fs::path file = basePath(userId, dbName) / "data" /
                    (collection + ".bin");

    if (fs::exists(file)) return;

    std::ofstream out(file, std::ios::binary);
    std::cout << "[ENGINE] Collection created: "
              << file << std::endl;
}

/* ---------------- INSERT ---------------- */

void DatabaseEngine::insert(const std::string& userId,
                            const std::string& dbName,
                            const std::string& collection,
                            const json& doc) {

    fs::path base = basePath(userId, dbName);
    fs::path dataFile = base / "data" / (collection + ".bin");
    fs::path walFile  = base / "wal/db.wal";

    if (!fs::exists(base))
        createDatabase(userId, dbName);

    if (!fs::exists(dataFile))
        createCollection(userId, dbName, collection);

    std::cout << "[ENGINE] INSERT\n";
    std::cout << "  DB   : " << dbName << "\n";
    std::cout << "  Coll : " << collection << "\n";
    std::cout << "  File : " << fs::absolute(dataFile) << std::endl;

    json walEntry = {
        {"op","INSERT"},
        {"userId",userId},
        {"db",dbName},
        {"collection",collection},
        {"data",doc}
    };

    WAL::log(walFile.string(), walEntry);
    Storage::appendDocument(dataFile.string(), doc);
    std::cout << "[STORAGE] Document written successfully\n";
}


//for finding one

std::vector<json> DatabaseEngine::find(
    const std::string& userId,
    const std::string& dbName,
    const std::string& collection,
    const json& filter) {

    fs::path file = fs::path(DATA_ROOT) / userId / dbName / "data" / (collection + ".bin");

    auto docs = Storage::readAll(file.string());
    std::vector<json> matches;

    QueryNode query = parseQuery(filter);

    for (auto& doc : docs)
        if (evalQuery(query, doc))
            matches.push_back(doc);

    std::cout << "[FIND] Matched " << matches.size() << " documents\n";

    return matches;
}

//for updating one
bool DatabaseEngine::updateOne(
    const std::string& userId,
    const std::string& dbName,
    const std::string& collection,
    const json& filter,
    const json& update
) {
    fs::path base = basePath(userId, dbName);

    fs::path file = base / "data" / (collection + ".bin");
    fs::path wal  = base / "wal/db.wal";

    auto docs = Storage::readAll(file.string());
    bool updated = false;

    for (auto& d : docs) {
      if (DatabaseEngine::match(d, filter)) {
    for (auto& [k,v] : update.items())
        d[k] = v;
    updated = true;
    break;
}
    
    }

    if (!updated) return false;

    json walEntry = {
        {"op","UPDATE"},
        {"userId",userId},
        {"db",dbName},
        {"collection",collection},
        {"filter",filter},
        {"update",update}
    };

    WAL::log(wal.string(), walEntry);

    std::ofstream out(file, std::ios::trunc);
    for (auto& d : docs) out << d.dump() << "\n";

    return true;
}



//for deleting one
bool DatabaseEngine::deleteOne(
    const std::string& userId,
    const std::string& dbName,
    const std::string& collection,
    const json& filter
) {
    fs::path base = basePath(userId, dbName);

    fs::path file = base / "data" / (collection + ".bin");
    fs::path wal  = base / "wal/db.wal";

    auto docs = Storage::readAll(file.string());
    std::vector<json> kept;
    bool deleted = false;

    for (auto& d : docs) {
        if (!deleted && match(d, filter)) {
            deleted = true;
            continue;
        }
        kept.push_back(d);
    }

    if (!deleted) return false;

    json walEntry = {
        {"op","DELETE"},
        {"userId",userId},
        {"db",dbName},
        {"collection",collection},
        {"filter",filter}
    };

    WAL::log(wal.string(), walEntry);

    std::ofstream out(file, std::ios::trunc);
    for (auto& d : kept) out << d.dump() << "\n";

    return true;
}

bool DatabaseEngine::match(nlohmann::json doc, nlohmann::json filter) {
    QueryNode query = parseQuery(filter);
    return evalQuery(query, doc);
}