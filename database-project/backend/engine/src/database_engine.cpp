#include "database_engine.hpp"
#include "storage.hpp"
#include "wal.hpp"
#include "logger.hpp"
#include "query.hpp"
#include "update_ops.hpp"

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

/* ---------------- DATABASE ---------------- */
void DatabaseEngine::createDatabase(const std::string& userId,
                                    const std::string& dbName) {
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

/* ---------------- INSERT ---------------- */
void DatabaseEngine::insert(const std::string& userId,
                            const std::string& dbName,
                            const std::string& collection,
                            const json& doc) {

    fs::path base = basePath(userId, dbName);
    fs::path dataFile = base / "data" / (collection + ".bin");
    fs::path walFile  = base / "wal/db.wal";

    if (!fs::exists(base)) createDatabase(userId, dbName);
    if (!fs::exists(dataFile)) createCollection(userId, dbName, collection);

    json walEntry = {
        {"op","INSERT"},
        {"userId",userId},
        {"db",dbName},
        {"collection",collection},
        {"data",doc}
    };

    WAL::log(walFile.string(), walEntry);

    std::cout << "[ENGINE][INSERT] " << doc.dump() << "\n";
    Storage::appendDocument(dataFile.string(), doc);
}

/* ---------------- FIND ---------------- */
std::vector<json> DatabaseEngine::find(const std::string& userId,
                                       const std::string& dbName,
                                       const std::string& collection,
                                       const json& filter) {

    fs::path file = basePath(userId, dbName) / "data" / (collection + ".bin");
    auto docs = Storage::readAll(file.string());

    std::vector<json> matches;
    for (auto& d : docs) {
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
    fs::path base     = basePath(userId, dbName);
    fs::path dataFile = base / "data" / (collection + ".bin");
    fs::path walFile  = base / "wal/db.wal";

    if (!fs::exists(base)) createDatabase(userId, dbName);
    if (!fs::exists(dataFile)) createCollection(userId, dbName, collection);

    auto docs = Storage::readAll(dataFile.string());
    bool updated = false;

    for (auto& d : docs) {
        if (match(d, filter)) {

            std::cout << "[ENGINE][UPDATE][BEFORE] " << d.dump() << "\n";

            /* 🔥 BACKWARD COMPATIBILITY FIX */
            bool hasOperator = false;
            for (auto& [k, _] : update.items()) {
                if (!k.empty() && k[0] == '$') {
                    hasOperator = true;
                    break;
                }
            }

            json effectiveUpdate;
            if (hasOperator) {
                effectiveUpdate = update;
                std::cout << "[ENGINE][UPDATE] Operator update detected\n";
            } else {
                effectiveUpdate = json{{"$set", update}};
                std::cout << "[ENGINE][UPDATE] Plain update → auto $set\n";
            }

            applyUpdateOps(d, effectiveUpdate);

            std::cout << "[ENGINE][UPDATE][AFTER] " << d.dump() << "\n";

            updated = true;
            break;
        }
    }

    if (!updated) {
        std::cout << "[ENGINE][UPDATE] No match for filter "
                  << filter.dump() << "\n";
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

    std::cout << "[ENGINE][UPDATE] Update persisted successfully\n";
    return true;
}

/* ---------------- DELETE ---------------- */
bool DatabaseEngine::deleteOne(const std::string& userId,
                               const std::string& dbName,
                               const std::string& collection,
                               const json& filter) {

    fs::path file = basePath(userId, dbName) / "data" / (collection + ".bin");
    fs::path walFile = basePath(userId, dbName) / "wal/db.wal";

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

/* ---------------- MATCH ---------------- */
bool DatabaseEngine::match(json doc, json filter) {
    if (filter.is_null() || filter.empty()) return true;
    QueryNode query = parseQuery(filter);
    return evalQuery(query, doc);
}
