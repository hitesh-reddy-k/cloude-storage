#pragma once
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

using json = nlohmann::json;

class LSM {
public:
    // initialize with engine data root
    static void init(const std::string& rootPath);

    // put document into memtable (and WAL) and schedule flush
    static void put(const std::string& userId,
                    const std::string& dbName,
                    const std::string& collection,
                    const json& doc);

    // read all documents (merge memtable + SST files)
    static std::vector<json> getAll(const std::string& userId,
                                    const std::string& dbName,
                                    const std::string& collection);

    // force a flush for a specific collection (debug)
    static void flush(const std::string& userId,
                      const std::string& dbName,
                      const std::string& collection);

    // COMPACTION & MAINTENANCE
    // run compaction for a collection (tiered compaction)
    static void compact(const std::string& userId,
                        const std::string& dbName,
                        const std::string& collection);

    // Bloom filter support (adaptive)
    static void buildBloomForSST(const std::string& sstPath);
    static bool mayExistInSST(const std::string& sstPath, const std::string& key);

    // Columnar secondary index scaffold
    static void updateColumnIndexes(const std::string& userId,
                                    const std::string& dbName,
                                    const std::string& collection,
                                    const json& doc);

    // start/stop background maintenance thread
    static void startBackgroundTasks();
    static void stopBackgroundTasks();
};
