#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <cstdint>

enum class TxState {
    ACTIVE,
    COMMITTED,
    ABORTED
};

struct Transaction {
    uint64_t id;
    TxState state;
    std::vector<std::string> walBuffer;
};

class TransactionManager {
public:
    Transaction& begin();
    void commit(Transaction& tx, const std::string& walFile);
    void rollback(Transaction& tx);

private:
    uint64_t genTxId();

    std::mutex txMutex;
    uint64_t txCounter = 0;
    std::unordered_map<uint64_t, Transaction> activeTx;
};
