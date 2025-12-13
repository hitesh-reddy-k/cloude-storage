#include "database_engine.hpp"
#include "storage.hpp"
#include "wal.hpp"
#include "logger.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;


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

    fs::path file = basePath() + userId + "/" + dbName +
                    "/data/" + collection + ".bin";

    std::cout << "[ENGINE] Creating collection file: "
              << file << std::endl;

    std::ofstream out(file, std::ios::binary | std::ios::app);

    if (!out.is_open()) {
        std::cout << "[ERROR] Failed to create collection" << std::endl;
        return;
    }

    std::cout << "[ENGINE] Collection created: "
              << collection << std::endl;
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
