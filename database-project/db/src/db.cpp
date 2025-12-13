#include "db.hpp"
#include <iostream>
#include <fstream>

using namespace std;

DB::DB(const string& wal_file) : wal(wal_file) {
    cout << "[DB] Loading WAL...\n";
    wal.replay(kv);
    loadCollections();
}

DB::~DB() {
    cout << "[DB] Saving data...\n";

    ofstream out("db.data");
    for (auto& p : kv)
        out << p.first << "=" << p.second << "\n";

    saveCollections();
}



void DB::set(const string& key, const string& value) {
    kv[key] = value;
    wal.appendSet(key, value);
}

string DB::get(const string& key) {
    auto it = kv.find(key);
    return (it != kv.end()) ? it->second : "";
}

void DB::del(const string& key) {
    kv.erase(key);
    wal.appendDelete(key);
}

// ---------------- Mongo-like ----------------

void DB::insertOne(const string& collection, const json& doc) {
    collections[collection].push_back(doc);
    cout << "[DB] insertOne -> " << doc.dump() << endl;
    saveCollections();
}

vector<json> DB::find(const string& collection, const json& query) {
    vector<json> result;

    auto it = collections.find(collection);
    if (it == collections.end()) return result;

    for (auto& doc : it->second) {
        bool match = true;
        for (auto& q : query.items()) {
            if (!doc.contains(q.key()) || doc[q.key()] != q.value()) {
                match = false;
                break;
            }
        }
        if (match) result.push_back(doc);
    }
    return result;
}

bool DB::updateOne(const string& collection, const json& query, const json& update) {
    auto it = collections.find(collection);
    if (it == collections.end()) return false;

    for (auto& doc : it->second) {
        bool match = true;
        for (auto& q : query.items()) {
            if (!doc.contains(q.key()) || doc[q.key()] != q.value()) {
                match = false;
                break;
            }
        }
        if (match) {
            for (auto& u : update.items())
                doc[u.key()] = u.value();

            saveCollections();
            return true;
        }
    }
    return false;
}

void DB::saveCollections() {
    ofstream out("collections.json");
    json j;
    for (auto& c : collections)
        j[c.first] = c.second;
    out << j.dump(4);
}

void DB::loadCollections() {
    ifstream in("collections.json");
    if (!in.is_open()) return;

    json j;
    in >> j;
    for (auto& c : j.items())
        collections[c.key()] = c.value().get<vector<json>>();
}
