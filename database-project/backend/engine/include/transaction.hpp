#pragma once
#include <vector>
#include <string>

struct Transaction {
    std::string id;
    std::vector<std::string> ops;
};
