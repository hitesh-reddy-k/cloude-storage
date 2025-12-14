#include "transaction_manager.hpp"
#include "wal.hpp"

#include <windows.h>
#include <rpc.h>

#include <string>
#include <mutex>
#include <unordered_map>

#pragma comment(lib, "Rpcrt4.lib")

// ---------- UUID generator (optional) ----------
std::string generateUUID() {
    UUID uuid;
    UuidCreate(&uuid);

    RPC_CSTR str;
    UuidToStringA(&uuid, &str);

    std::string uuidStr(reinterpret_cast<char*>(str));
    RpcStringFreeA(&str);

    return uuidStr;
}

// ---------- Transaction ID generator ----------
uint64_t TransactionManager::genTxId() {
    return ++txCounter;
}

// ---------- BEGIN ----------
Transaction& TransactionManager::begin() {
    std::lock_guard<std::mutex> lock(txMutex);

    Transaction tx;
    tx.id = genTxId();
    tx.state = TxState::ACTIVE;

    // Insert directly and return reference
    auto result = activeTx.emplace(tx.id, std::move(tx));
    return result.first->second;
}

// ---------- COMMIT ----------
void TransactionManager::commit(Transaction& tx,
                                const std::string& walFile) {
    for (const auto& entry : tx.walBuffer) {
        WAL::log(walFile, entry);
    }

    tx.state = TxState::COMMITTED;

    std::lock_guard<std::mutex> lock(txMutex);
    activeTx.erase(tx.id);
}

// ---------- ROLLBACK ----------
void TransactionManager::rollback(Transaction& tx) {
    tx.walBuffer.clear();
    tx.state = TxState::ABORTED;

    std::lock_guard<std::mutex> lock(txMutex);
    activeTx.erase(tx.id);
}
