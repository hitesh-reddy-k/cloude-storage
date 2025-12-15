    #include "storage.hpp"
    #include <fstream>
    #include <iostream>

    using json = nlohmann::json;

    void Storage::appendDocument(const std::string& file, const json& doc) {
        std::ofstream out(file, std::ios::binary | std::ios::app);

        std::string data = doc.dump();
        uint32_t len = static_cast<uint32_t>(data.size());

        out.write(reinterpret_cast<char*>(&len), sizeof(len));
        out.write(data.data(), len);
    }


    std::vector<json> Storage::readAll(const std::string& file) {
        std::ifstream in(file, std::ios::binary);
        std::vector<json> docs;

        while (true) {
            uint32_t len;
            if (!in.read(reinterpret_cast<char*>(&len), sizeof(len)))
                break;

            std::string buf(len, '\0');
            in.read(&buf[0], len);

            docs.push_back(json::parse(buf));
        }

        return docs;
    }

