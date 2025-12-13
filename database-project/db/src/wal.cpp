#include "wal.hpp"
#include <iostream>
#include <chrono>

using namespace std;
using namespace std::chrono;

void WAL::writeString(ofstream& w, const string& s) {
    uint32_t len = s.size();
    w.write((char*)&len, 4);
    w.write(s.data(), len);
}

WAL::WAL(const string& filename) : filename(filename) {
    cout << "[WAL] Opening WAL file: " << filename << endl;
    writer.open(filename, ios::binary | ios::app);
    if (!writer.is_open())
        cout << "[WAL] ERROR: Could not open WAL file!" << endl;
    else
        cout << "[WAL] WAL opened successfully." << endl;
}

void WAL::appendSet(const string& key, const string& value) {
    auto start = high_resolution_clock::now();

    char op = 1;
    writer.write(&op, 1);
    writeString(writer, key);
    writeString(writer, value);
    writer.flush();

    auto end = high_resolution_clock::now();
    double micro = duration<double, std::micro>(end - start).count();
    size_t bytes = 1 + 4 + key.size() + 4 + value.size();
    cout << "[WAL] Appended SET: key=[" << key << "] val=[" << value << "] bytes=" << bytes << " time=" << micro << " µs\n";
}

void WAL::appendDelete(const string& key) {
    auto start = high_resolution_clock::now();

    char op = 2;
    writer.write(&op, 1);
    writeString(writer, key);
    writer.flush();

    auto end = high_resolution_clock::now();
    double micro = duration<double, std::micro>(end - start).count();
    size_t bytes = 1 + 4 + key.size();
    cout << "[WAL] Appended DELETE: key=[" << key << "] bytes=" << bytes << " time=" << micro << " µs\n";
}

void WAL::replay(unordered_map<string, string>& memtable) {
    cout << "[WAL] Starting WAL replay...\n";
    auto start = high_resolution_clock::now();

    ifstream reader(filename, ios::binary);
    if (!reader.is_open()) {
        cout << "[WAL] No WAL file found, skipping replay.\n";
        return;
    }

    size_t recordCount = 0;
    while (true) {
        char op;
        if (!reader.read(&op, 1)) break;

        uint32_t keyLen;
        if (!reader.read((char*)&keyLen, 4)) break;
        string key(keyLen, '\0');
        reader.read(key.data(), keyLen);

        if (op == 1) { // SET
            uint32_t valLen;
            reader.read((char*)&valLen, 4);
            string val(valLen, '\0');
            reader.read(val.data(), valLen);
            memtable[key] = val;
            cout << "[WAL] Replayed SET key=[" << key << "] value=[" << val << "]\n";
        } else if (op == 2) { // DELETE
            memtable.erase(key);
            cout << "[WAL] Replayed DELETE key=[" << key << "]\n";
        }
        recordCount++;
    }

    auto end = high_resolution_clock::now();
    double ms = duration<double, milli>(end - start).count();
    cout << "[WAL] Replay finished. Records=" << recordCount << " Time=" << ms << " ms\n";
}
