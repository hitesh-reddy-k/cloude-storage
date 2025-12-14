#include "database_engine.hpp"
#include "storage.hpp"
#include "wal.hpp"
#include "logger.hpp"
#include <vector>
#include "transaction.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;

static bool pathExists(const std::filesystem::path& p) {
    return std::filesystem::exists(p);
}


using json = nlohmann::json;
static std::string DATA_ROOT = "../../data/databases/";

void DatabaseEngine::init(const std::string& rootPath) {
    DATA_ROOT = rootPath;

    if (!DATA_ROOT.empty()) {
        char last = DATA_ROOT.back();
        if (last != '/' && last != '\\') {
            DATA_ROOT += "/";
        }
    }

    std::cout << "[ENGINE] Data root initialized: "
              << DATA_ROOT << std::endl;

    // ✅ WAL crash recovery
    std::cout << "[ENGINE] Replaying WAL if exists...\n";
    WAL::replay(DATA_ROOT);
}


static std::string basePath() {
    return DATA_ROOT;
}

void DatabaseEngine::createDatabase(const std::string& userId, const std::string& dbName) {

    fs::path dbPath = basePath() + userId + "/" + dbName;

    std::cout << "[DEBUG] Absolute DB path: "
          << fs::absolute(dbPath) << std::endl;

    std::cout << "[ENGINE] Creating database at: "
              << dbPath << std::endl;

    fs::create_directories(dbPath / "data");
    fs::create_directories(dbPath / "wal");
    fs::create_directories(dbPath / "logs");

    Logger::write((dbPath / "logs/db.log").string(),
                  "DATABASE CREATED");

    std::cout << "[ENGINE] Database created successfully" << std::endl;
}

void DatabaseEngine::createCollection(const std::string& userId,
                                      const std::string& dbName,
                                      const std::string& collection) {

    fs::path dbPath = basePath() + userId + "/" + dbName;

    if (!pathExists(dbPath)) {
        std::cout << "[ERROR] Database does not exist: "
                  << dbName << std::endl;
        return;
    }

    fs::path file = dbPath / "data" / (collection + ".bin");

    if (pathExists(file)) {
        std::cout << "[ERROR] Collection already exists: "
                  << collection << std::endl;
        return;
    }

    std::cout << "[ENGINE] Creating collection: "
              << file << std::endl;

    std::ofstream out(file, std::ios::binary);

    if (!out.is_open()) {
        std::cout << "[ERROR] Failed to create collection file" << std::endl;
        return;
    }

    std::cout << "[ENGINE] Collection created successfully\n";
}




void DatabaseEngine::insert(const std::string& userId,
                            const std::string& dbName,
                            const std::string& collection,
                            const json& doc) {

    fs::path base = basePath() + userId + "/" + dbName;
    fs::path dataFile = base / "data" / (collection + ".bin");
    fs::path walFile  = base / "wal/db.wal";
    fs::path logFile  = base / "logs/db.log";

    std::cout << "[ENGINE] Insert requested" << std::endl;
    std::cout << "  User: " << userId << std::endl;
    std::cout << "  DB  : " << dbName << std::endl;
    
    std::cout << "  Coll: " << collection << std::endl;
    std::cout << "  Doc : " << doc.dump() << std::endl;

    WAL::log(walFile.string(), doc.dump());
    Storage::appendDocument(dataFile.string(), doc);
    Logger::write(logFile.string(), "INSERT " + doc.dump());

    std::cout << "[ENGINE] Insert completed successfully" << std::endl;
}

//for finding one

std::vector<json> DatabaseEngine::find(
    const std::string& userId,
    const std::string& dbName,
    const std::string& collection,
    const json& filter) {

    fs::path file = basePath()+userId+"/"+dbName+
                    "/data/"+collection+".bin";

    std::vector<json> all = Storage::readAll(file.string());
    std::vector<json> matches;

    for (auto& doc : all) {
        bool ok = true;
        for (auto& [k,v] : filter.items()) {
            if (!doc.contains(k) || doc[k] != v) {
                ok = false;
                break;
            }
        }
        if (ok) matches.push_back(doc);
    }

    std::cout << "[FIND] Matched " << matches.size()
              << " documents\n";

    return matches;
}

//for updating one

bool DatabaseEngine::updateOne(
    const std::string& userId,
    const std::string& dbName,
    const std::string& collection,
    const json& filter,
    const json& update) {

    fs::path file = basePath()+userId+"/"+dbName+
                    "/data/"+collection+".bin";

    auto docs = Storage::readAll(file.string());
    bool updated = false;

    for (auto& doc : docs) {
        bool match = true;
        for (auto& [k,v] : filter.items())
            if (doc[k] != v) match = false;

        if (match) {
            for (auto& [k,v] : update.items())
                doc[k] = v;
            updated = true;
            break;
        }
    }

    if (!updated) {
        std::cout << "[UPDATE] No match found\n";
        return false;
    }

    std::ofstream out(file, std::ios::binary | std::ios::trunc);
    for (auto& d : docs)
        out << d.dump() << "\n";

    std::cout << "[UPDATE] One document updated\n";
    return true;
}



//for deleting one
bool DatabaseEngine::deleteOne(
    const std::string& userId,
    const std::string& dbName,
    const std::string& collection,
    const json& filter) {

    fs::path file = basePath()+userId+"/"+dbName+
                    "/data/"+collection+".bin";

    auto docs = Storage::readAll(file.string());
    std::vector<json> kept;
    bool deleted = false;

    for (auto& doc : docs) {
        bool match = true;
        for (auto& [k,v] : filter.items())
            if (doc[k] != v) match = false;

        if (!match || deleted)
            kept.push_back(doc);
        else
            deleted = true;
    }

    if (!deleted) {
        std::cout << "[DELETE] No match found\n";
        return false;
    }

    std::ofstream out(file, std::ios::trunc);
    for (auto& d : kept)
        out << d.dump() << "\n";

    std::cout << "[DELETE] One document removed\n";
    return true;
}


bool DatabaseEngine::match(json doc, json filter) {
    for (auto& [k, v] : filter.items()) {
        if (v.is_object()) {
            if (v.contains("$gt") && doc[k] <= v["$gt"]) return false;
            if (v.contains("$lt") && doc[k] >= v["$lt"]) return false;
        } else {
            if (doc[k] != v) return false;
        }
    }
    return true;
}


Transaction tx;

void begin() {
    tx.id = "txn123";
    WAL::log("data/db.wal", "BEGIN " + tx.id);
}

void commit() {
    WAL::log("data/db.wal", "COMMIT " + tx.id);
}

void rollback() {
    WAL::log("data/db.wal", "ROLLBACK " + tx.id);
}

