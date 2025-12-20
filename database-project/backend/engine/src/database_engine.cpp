#include "database_engine.hpp"
#include "storage.hpp"
#include "wal.hpp"
#include "logger.hpp"
#include "query.hpp"
#include "update_ops.hpp"
#include "lsm.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;
using json = nlohmann::json;

static std::string DATA_ROOT;

/* ---------------- INIT ---------------- */
void DatabaseEngine::init(const std::string& rootPath) {
    DATA_ROOT = rootPath;
    if (DATA_ROOT.back() != '/' && DATA_ROOT.back() != '\\')
        DATA_ROOT += "/";

    fs::create_directories(DATA_ROOT);
    std::cout << "[ENGINE] Data root initialized: "
              << fs::absolute(DATA_ROOT) << std::endl;
}

static fs::path basePath(const std::string& userId, const std::string& dbName) {
    return fs::path(DATA_ROOT) / userId / dbName;
}

/* ---------------- USER ROOT ---------------- */
void DatabaseEngine::ensureUserRoot(const std::string& userId) {
    fs::path userRoot = fs::path(DATA_ROOT) / userId;
    fs::create_directories(userRoot);
    fs::create_directories(userRoot / "data");
    fs::create_directories(userRoot / "wal");
    fs::create_directories(userRoot / "logs");
}

/* ---------------- DATABASE ---------------- */
void DatabaseEngine::createDatabase(const std::string& userId,
                                    const std::string& dbName) {
    ensureUserRoot(userId);
    fs::path base = basePath(userId, dbName);
    fs::create_directories(base / "data");
    fs::create_directories(base / "wal");
    fs::create_directories(base / "logs");
    Logger::write((base / "logs/db.log").string(), "DATABASE CREATED");
}

/* ---------------- COLLECTION ---------------- */
void DatabaseEngine::createCollection(const std::string& userId,
                                      const std::string& dbName,
                                      const std::string& collection) {
    fs::path file = basePath(userId, dbName) / "data" / (collection + ".bin");
    if (fs::exists(file)) return;

    std::ofstream out(file, std::ios::binary);
    std::cout << "[ENGINE][CREATE COLLECTION] " << file.string() << "\n";
}

/* ---------------- LIST DATABASES ---------------- */
std::vector<std::string> DatabaseEngine::listDatabases(const std::string& userId) {
    std::vector<std::string> names;

    fs::path userRoot = fs::path(DATA_ROOT) / userId;
    if (!fs::exists(userRoot)) return names;

    for (const auto& entry : fs::directory_iterator(userRoot)) {
        if (entry.is_directory()) {
            names.push_back(entry.path().filename().string());
        }
    }

    return names;
}

/* ---------------- INSERT ---------------- */
void DatabaseEngine::insert(const std::string& userId,
                            const std::string& dbName,
                            const std::string& collection,
                            const json& doc) {

    // If this is the main system users collection, persist in the .bin storage
    if (!fs::exists(basePath(userId, dbName))) createDatabase(userId, dbName);

    bool isMainUsers = (dbName == "system" && collection == "users");

    if (isMainUsers) {
        fs::path dataFile = basePath(userId, dbName) / "data" / (collection + ".bin");
        fs::path walFile  = basePath(userId, dbName) / "wal/db.wal";

        // append to storage and write WAL
        Storage::appendDocument(dataFile.string(), doc);

        json walEntry = {
            {"op", "INSERT"},
            {"userId", userId},
            {"db", dbName},
            {"collection", collection},
            {"data", doc}
        };
        WAL::log(walFile.string(), walEntry);

        std::cout << "[ENGINE] Inserted into .bin storage for system users\n";
        return;
    }

    // delegate to LSM layer (which will write WAL, memtable and flush to SST)
    std::cout << "[ENGINE] Using LSM::put for insert\n";
    LSM::put(userId, dbName, collection, doc);
}

/* ---------------- FIND ---------------- */
std::vector<json> DatabaseEngine::find(const std::string& userId,
                                       const std::string& dbName,
                                       const std::string& collection,
                                       const json& filter) {
    bool isMainUsers = (dbName == "system" && collection == "users");
    std::vector<json> docs;

    if (isMainUsers) {
        fs::path file = basePath(userId, dbName) / "data" / (collection + ".bin");
        docs = Storage::readAll(file.string());
    } else {
        // read via LSM layer (merge memtable + SSTs)
        docs = LSM::getAll(userId, dbName, collection);
    }

    std::vector<json> matches;
    for (auto& d : docs) {
        // skip tombstones produced by LSM deletes
        if (d.contains("_deleted") && d["_deleted"].is_boolean() && d["_deleted"].get<bool>()) continue;

        if (match(d, filter)) {
            matches.push_back(d);
            std::cout << "[ENGINE][FIND MATCH] " << d.dump() << "\n";
        }
    }

    std::cout << "[ENGINE][FIND] Matched " << matches.size()
              << " / " << docs.size() << "\n";

    return matches;
}

/* ---------------- UPDATE ---------------- */
bool DatabaseEngine::updateOne(
    const std::string& userId,
    const std::string& dbName,
    const std::string& collection,
    const json& filter,
    const json& update
) {
    fs::path base = basePath(userId, dbName);
    if (!fs::exists(base)) createDatabase(userId, dbName);
    bool isMainUsers = (dbName == "system" && collection == "users");

    if (isMainUsers) {
        fs::path dataFile = base / "data" / (collection + ".bin");
        fs::path walFile = base / "wal/db.wal";

        auto docs = Storage::readAll(dataFile.string());
        bool updated = false;

        for (auto& d : docs) {
            if (match(d, filter)) {
                std::cout << "[ENGINE][UPDATE][BEFORE] " << d.dump() << "\n";

                bool hasOperator = false;
                for (auto& [k, _] : update.items()) {
                    if (!k.empty() && k[0] == '$') { hasOperator = true; break; }
                }

                json effectiveUpdate = hasOperator ? update : json{{"$set", update}};
                if (!hasOperator) std::cout << "[ENGINE][UPDATE] Plain update → auto $set\n";

                applyUpdateOps(d, effectiveUpdate);

                std::cout << "[ENGINE][UPDATE][AFTER] " << d.dump() << "\n";
                updated = true;
                break;
            }
        }

        if (!updated) {
            std::cout << "[ENGINE][UPDATE] No match for filter " << filter.dump() << "\n";
            return false;
        }

        json walEntry = {
            {"op", "UPDATE"},
            {"userId", userId},
            {"db", dbName},
            {"collection", collection},
            {"filter", filter},
            {"update", update}
        };

        WAL::log(walFile.string(), walEntry);
        Storage::writeAll(dataFile.string(), docs);
        std::cout << "[ENGINE][UPDATE] Update persisted to .bin storage\n";
        return true;
    }

    // fallback to LSM path for regular collections
    auto docs = LSM::getAll(userId, dbName, collection);
    bool updated = false; json updatedDoc;

    for (auto& d : docs) {
        if (match(d, filter)) {
            std::cout << "[ENGINE][UPDATE][BEFORE] " << d.dump() << "\n";
            bool hasOperator = false;
            for (auto& [k, _] : update.items()) {
                if (!k.empty() && k[0] == '$') { hasOperator = true; break; }
            }
            json effectiveUpdate = hasOperator ? update : json{{"$set", update}};
            applyUpdateOps(d, effectiveUpdate);
            std::cout << "[ENGINE][UPDATE][AFTER] " << d.dump() << "\n";
            updated = true; updatedDoc = d; break;
        }
    }

    if (!updated) { std::cout << "[ENGINE][UPDATE] No match for filter " << filter.dump() << "\n"; return false; }
    if (!updatedDoc.contains("id")) { std::cout << "[ENGINE][UPDATE] Missing id field, skipping update write\n"; return false; }
    LSM::put(userId, dbName, collection, updatedDoc);
    std::cout << "[ENGINE][UPDATE] Update persisted via LSM\n";
    return true;
}

/* ---------------- DELETE ---------------- */
bool DatabaseEngine::deleteOne(const std::string& userId,
                               const std::string& dbName,
                               const std::string& collection,
                               const json& filter) {

    fs::path base = basePath(userId, dbName);
    if (!fs::exists(base)) createDatabase(userId, dbName);

    bool isMainUsers = (dbName == "system" && collection == "users");

    if (isMainUsers) {
        fs::path file = base / "data" / (collection + ".bin");
        fs::path walFile = base / "wal/db.wal";

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

        WAL::log(walFile.string(), walEntry);
        Storage::writeAll(file.string(), kept);

        std::cout << "[ENGINE][DELETE] Success\n";
        return true;
    }

    // LSM-backed collection: locate matching document, then write a tombstone
    auto docs = LSM::getAll(userId, dbName, collection);
    bool found = false; std::string targetId;

    for (auto& d : docs) {
        // skip LSM tombstones when searching
        if (d.contains("_deleted") && d["_deleted"].is_boolean() && d["_deleted"].get<bool>()) continue;
        if (match(d, filter)) {
            if (d.contains("id")) {
                targetId = d["id"].get<std::string>();
                found = true;
                break;
            }
        }
    }

    if (!found) {
        std::cout << "[ENGINE][DELETE] No match for filter " << filter.dump() << "\n";
        return false;
    }

    // write tombstone via LSM::del (which logs a DELETE and inserts tombstone into memtable)
    LSM::del(userId, dbName, collection, targetId);

    std::cout << "[ENGINE][DELETE] Tombstone written for id=" << targetId << "\n";
    return true;
}

/* ---------------- MATCH ---------------- */
bool DatabaseEngine::match(const json& doc, const json& filter)
{
    if (filter.is_null() || filter.empty()) return true;
    QueryNode query = parseQuery(filter);
    return evalQuery(query, doc);
}
