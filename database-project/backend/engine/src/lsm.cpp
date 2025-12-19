#include "lsm.hpp"
#include "wal.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <mutex>
#include <unordered_map>
#include <thread>
#include <chrono>
#include <functional>
#include <unordered_set>
#include <atomic>

namespace fs = std::filesystem;

static std::mutex lsm_mutex;

// memtable keyed by collectionPath -> map<id,json>
static std::unordered_map<std::string, std::unordered_map<std::string, json>> memtables;
static std::string LSM_ROOT;
static const size_t MEMTABLE_LIMIT = 8; // small for testing
static const size_t COMPACTION_THRESHOLD = 2; // number of SSTs to compact
static std::atomic<bool> bgRunning(false);

// simple bloom params
static const size_t BLOOM_SIZE = 1024; // bits

static std::thread bgThread;

void LSM::init(const std::string& rootPath) {
    LSM_ROOT = rootPath;
    if (LSM_ROOT.back() != '/' && LSM_ROOT.back() != '\\') LSM_ROOT += "/";
    std::cout << "[LSM] Initialized at: " << LSM_ROOT << std::endl;
}

static std::string colKey(const std::string& userId, const std::string& db, const std::string& coll) {
    return userId + "/" + db + "/" + coll;
}

void LSM::put(const std::string& userId, const std::string& dbName, const std::string& collection, const json& doc) {
    std::lock_guard<std::mutex> lk(lsm_mutex);
    std::string key = colKey(userId, dbName, collection);

    // ensure directory
    fs::path dir = fs::path(LSM_ROOT) / userId / dbName / (collection + ".lsm");
    fs::create_directories(dir);

    // WAL entry
    json walEntry = {
        {"op","PUT"},
        {"userId", userId},
        {"db", dbName},
        {"collection", collection},
        {"data", doc}
    };
    std::string walFile = (fs::path(LSM_ROOT) / userId / dbName / "wal" / (collection + ".wal")).string();
    WAL::log(walFile, walEntry);

    // memtable insert (use id if present)
    std::string id = doc.contains("id") ? doc["id"].get<std::string>() : std::to_string(std::time(nullptr));
    memtables[key][id] = doc;

    std::cout << "[LSM][PUT] " << key << " / id=" << id << "\n";

    if (memtables[key].size() >= MEMTABLE_LIMIT) {
        std::cout << "[LSM] memtable threshold reached, flushing..." << std::endl;
        LSM::flush(userId, dbName, collection);
    }
}

void LSM::flush(const std::string& userId, const std::string& dbName, const std::string& collection) {
    std::lock_guard<std::mutex> lk(lsm_mutex);
    std::string key = colKey(userId, dbName, collection);
    fs::path dir = fs::path(LSM_ROOT) / userId / dbName / (collection + ".lsm");
    fs::create_directories(dir);

    if (memtables.find(key) == memtables.end() || memtables[key].empty()) {
        std::cout << "[LSM][FLUSH] memtable empty for " << key << std::endl;
        return;
    }

    // create SST file
    std::string sstName = std::to_string(std::time(nullptr)) + ".sst";
    fs::path sstPath = dir / sstName;
    std::ofstream out(sstPath.string(), std::ios::trunc);
    if (!out.is_open()) {
        std::cerr << "[LSM][FLUSH] cannot open sst file: " << sstPath << std::endl;
        return;
    }

    for (auto& [id, doc] : memtables[key]) {
        out << doc.dump() << "\n";
    }
    out.flush();
    out.close();

    std::cout << "[LSM][FLUSH] Wrote " << memtables[key].size() << " entries to " << sstPath.string() << std::endl;

    memtables[key].clear();
    // update simple column indexes for flushed SST
    updateColumnIndexes(userId, dbName, collection, json::object());

}

// ---------------- COMPACTION ----------------
void LSM::compact(const std::string& userId, const std::string& dbName, const std::string& collection) {
    std::lock_guard<std::mutex> lk(lsm_mutex);
    fs::path dir = fs::path(LSM_ROOT) / userId / dbName / (collection + ".lsm");
    if (!fs::exists(dir)) return;

    // collect sst files
    std::vector<fs::path> ssts;
    for (auto& e : fs::directory_iterator(dir)) if (e.path().extension() == ".sst") ssts.push_back(e.path());

    if (ssts.size() < COMPACTION_THRESHOLD) return;

    // simple merge: read all docs and write single merged sst
    std::unordered_map<std::string, json> merged;
    for (auto& p : ssts) {
        std::ifstream in(p.string());
        std::string line;
        while (std::getline(in, line)) {
            try {
                auto j = json::parse(line);
                std::string id = j.contains("id") ? j["id"].get<std::string>() : std::to_string(std::time(nullptr));
                merged[id] = j; // last-one-wins
            } catch (...) { }
        }
    }

    // write merged sst
    std::string outName = std::to_string(std::time(nullptr)) + ".sst";
    fs::path outPath = dir / outName;
    std::ofstream out(outPath.string(), std::ios::trunc);
    for (auto& [id, j] : merged) out << j.dump() << "\n";
    out.flush(); out.close();

    // remove old ssts
    for (auto& p : ssts) fs::remove(p);

    std::cout << "[LSM][COMPACT] Merged " << ssts.size() << " SSTs into " << outPath.string() << std::endl;
}

// ---------------- BLOOM FILTER (VERY SIMPLE) ----------------
void LSM::buildBloomForSST(const std::string& sstPath) {
    try {
        std::ifstream in(sstPath);
        if (!in.is_open()) return;
        std::vector<uint64_t> bits(BLOOM_SIZE/64);
        std::string line;
        while (std::getline(in, line)) {
            try {
                auto j = json::parse(line);
                std::string id = j.contains("id") ? j["id"].get<std::string>() : line;
                uint64_t h = std::hash<std::string>{}(id);
                size_t idx = h % BLOOM_SIZE;
                bits[idx/64] |= (1ULL << (idx%64));
            } catch (...) {}
        }

        // write bloom as json array
        std::string bloomPath = sstPath + std::string(".bloom");
        std::ofstream out(bloomPath, std::ios::trunc);
        json b = json::array();
        for (auto v : bits) b.push_back(v);
        out << b.dump();
        out.close();
        std::cout << "[LSM][BLOOM] Built bloom for " << sstPath << std::endl;
    } catch (...) {}
}

bool LSM::mayExistInSST(const std::string& sstPath, const std::string& key) {
    try {
        std::string bloomPath = sstPath + std::string(".bloom");
        std::ifstream in(bloomPath);
        if (!in.is_open()) return true; // unknown -> check file
        json b; in >> b;
        uint64_t h = std::hash<std::string>{}(key);
        size_t idx = h % BLOOM_SIZE;
        uint64_t v = b[idx/64].get<uint64_t>();
        return (v >> (idx%64)) & 1ULL;
    } catch (...) { return true; }
}

// ---------------- COLUMNAR INDEX ----------------
void LSM::updateColumnIndexes(const std::string& userId, const std::string& dbName, const std::string& collection, const json& doc) {
    // simple scaffold: iterate memtable keys and write index files per field
    try {
        std::string key = colKey(userId, dbName, collection);
        fs::path idxDir = fs::path(LSM_ROOT) / userId / dbName / (collection + ".idx");
        fs::create_directories(idxDir);

        // scan memtable entries for this collection
        if (memtables.find(key) == memtables.end()) return;
        for (auto& [id, j] : memtables[key]) {
            for (auto& [k, v] : j.items()) {
                if (k == "id") continue;
                fs::path idxFile = idxDir / (k + ".json");
                json idx;
                if (fs::exists(idxFile)) {
                    std::ifstream in(idxFile.string()); in >> idx; in.close();
                }
                std::string sval = v.is_string() ? v.get<std::string>() : v.dump();
                idx[sval].push_back(id);
                std::ofstream out(idxFile.string(), std::ios::trunc);
                out << idx.dump(); out.close();
            }
        }
    } catch (...) {}
}

// ---------------- BACKGROUND TASKS ----------------
static void maintenanceLoop() {
    while (bgRunning.load()) {
        try {
            // iterate users and collections and compact when needed
            for (auto& u : fs::directory_iterator(LSM_ROOT)) {
                if (!u.is_directory()) continue;
                std::string userId = u.path().filename().string();
                for (auto& db : fs::directory_iterator(u.path())) {
                    if (!db.is_directory()) continue;
                    std::string dbName = db.path().filename().string();
                    for (auto& c : fs::directory_iterator(db.path())) {
                        if (!c.is_directory()) continue;
                        std::string coll = c.path().filename().string();
                        // only process .lsm dirs
                        if (coll.size() > 4 && coll.substr(coll.size()-4) == ".lsm") {
                            std::string collection = coll.substr(0, coll.size()-4);
                            LSM::compact(userId, dbName, collection);
                        }
                    }
                }
            }
        } catch (...) {}
        std::this_thread::sleep_for(std::chrono::seconds(10));
    }
}

void LSM::startBackgroundTasks() {
    if (bgRunning.exchange(true)) return;
    bgThread = std::thread(maintenanceLoop);
    bgThread.detach();
    std::cout << "[LSM] Background maintenance started" << std::endl;
}

void LSM::stopBackgroundTasks() {
    bgRunning.store(false);
    std::cout << "[LSM] Background maintenance stopping" << std::endl;
}
std::vector<json> LSM::getAll(const std::string& userId, const std::string& dbName, const std::string& collection) {
    std::lock_guard<std::mutex> lk(lsm_mutex);
    std::vector<json> outDocs;
    std::string key = colKey(userId, dbName, collection);
    fs::path dir = fs::path(LSM_ROOT) / userId / dbName / (collection + ".lsm");

    if (fs::exists(dir)) {
        for (auto& entry : fs::directory_iterator(dir)) {
            if (entry.path().extension() == ".sst") {
                std::ifstream in(entry.path().string());
                std::string line;
                while (std::getline(in, line)) {
                    try {
                        outDocs.push_back(json::parse(line));
                    } catch (...) {
                        std::cerr << "[LSM] corrupted sst line skipped" << std::endl;
                    }
                }
            }
        }
    }

    // overlay memtable (newer entries)
    if (memtables.find(key) != memtables.end()) {
        for (auto& [id, doc] : memtables[key]) outDocs.push_back(doc);
    }

    std::cout << "[LSM][GETALL] returning " << outDocs.size() << " docs for " << key << std::endl;
    return outDocs;
}
