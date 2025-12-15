#include "database_engine.hpp"
#include "storage.hpp"
#include "wal.hpp"
#include "logger.hpp"
#include "query.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;
using json = nlohmann::json;

static std::string DATA_ROOT;

void DatabaseEngine::init(const std::string& rootPath) {
    DATA_ROOT = rootPath;
    if (DATA_ROOT.back() != '/' && DATA_ROOT.back() != '\\')
        DATA_ROOT += "/";

    fs::create_directories(DATA_ROOT);
    std::cout << "[ENGINE] Data root initialized: " << fs::absolute(DATA_ROOT) << std::endl;
}

static fs::path basePath(const std::string& userId, const std::string& dbName) {
    return fs::path(DATA_ROOT) / userId / dbName;
}

/* ---------------- DATABASE ---------------- */
void DatabaseEngine::createDatabase(const std::string& userId, const std::string& dbName) {
    fs::path base = basePath(userId, dbName);
    fs::create_directories(base / "data");
    fs::create_directories(base / "wal");
    fs::create_directories(base / "logs");
    Logger::write((base / "logs/db.log").string(), "DATABASE CREATED");
}

/* ---------------- COLLECTION ---------------- */
void DatabaseEngine::createCollection(const std::string& userId, const std::string& dbName, const std::string& collection) {
    fs::path file = basePath(userId, dbName) / "data" / (collection + ".bin");
    if (fs::exists(file)) return;
    std::ofstream out(file, std::ios::binary);
    std::cout << "[ENGINE][CREATE COLLECTION] Created \"" << file.string() << "\"\n";
}

/* ---------------- INSERT ---------------- */
void DatabaseEngine::insert(const std::string& userId, const std::string& dbName,
                            const std::string& collection, const json& doc) {

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
    std::cout << "[ENGINE][INSERT] Inserting doc into \"" << dataFile.string() << "\": " << doc.dump() << "\n";
    Storage::appendDocument(dataFile.string(), doc);
}

/* ---------------- FIND ---------------- */
std::vector<json> DatabaseEngine::find(const std::string& userId, const std::string& dbName,
                                       const std::string& collection, const json& filter) {

    fs::path file = basePath(userId, dbName) / "data" / (collection + ".bin");
    auto docs = Storage::readAll(file.string());

    std::cout << "[STORAGE][READ] Read " << docs.size() << " docs from " << file.string() << "\n";
    for (auto& d : docs) std::cout << "[STORAGE][DOC] " << d.dump() << "\n";

    std::vector<json> matches;
    for (auto& doc : docs) {
        if (DatabaseEngine::match(doc, filter)) {  // use match() here
            matches.push_back(doc);
            std::cout << "[ENGINE][FIND MATCH] " << doc.dump() << "\n";
        }
    }

    std::cout << "[ENGINE][FIND] Docs before filtering: " << docs.size() << "\n";
    std::cout << "[ENGINE][FIND] Found " << matches.size() << " docs matching filter\n";

    return matches;
}

/* ---------------- UPDATE ---------------- */
bool DatabaseEngine::updateOne(const std::string& userId, const std::string& dbName,
                               const std::string& collection, const json& filter,
                               const json& update) {

    fs::path file = basePath(userId, dbName) / "data" / (collection + ".bin");
    fs::path walFile = basePath(userId, dbName) / "wal/db.wal";

    auto docs = Storage::readAll(file.string());
    bool updated = false;

    for (auto& d : docs) {
        if (match(d, filter)) {
            for (auto& [k,v] : update.items()) d[k] = v;
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
    WAL::log(walFile.string(), walEntry);

    Storage::writeAll(file.string(), docs);
    std::cout << "[ENGINE][UPDATE] Updated doc matching filter: " << filter.dump() << "\n";
    return true;
}

/* ---------------- DELETE ---------------- */
bool DatabaseEngine::deleteOne(const std::string& userId, const std::string& dbName,
                               const std::string& collection, const json& filter) {

    fs::path file = basePath(userId, dbName) / "data" / (collection + ".bin");
    fs::path walFile = basePath(userId, dbName) / "wal/db.wal";

    auto docs = Storage::readAll(file.string());
    std::vector<json> kept;
    bool deleted = false;

    std::cout << "[DELETE] Before delete, docs:\n";
    for (auto& d : docs) std::cout << d.dump() << "\n";

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
    std::cout << "[ENGINE][DELETE] Deleted doc matching filter: " << filter.dump() << "\n";

    std::cout << "[DELETE] After delete, kept docs:\n";
    for (auto& d : kept) std::cout << d.dump() << "\n";

    return true;
}

/* ---------------- MATCH ---------------- */
bool DatabaseEngine::match(json doc, json filter) {
    if (filter.is_null() || (filter.is_object() && filter.empty())) return true;
    QueryNode query = parseQuery(filter);
    return evalQuery(query, doc);
}
