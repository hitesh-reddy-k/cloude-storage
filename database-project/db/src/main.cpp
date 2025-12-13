#include <iostream>
#include <string>
#include "db.hpp"
#include <nlohmann/json.hpp>


using namespace std;
using json = nlohmann::json;

int main() {
    DB db("db.wal");
    string cmd;

    while (true) {
        cout << ">> ";
        cin >> cmd;

        if (cmd == "set") {
            string k, v;
            cin >> k >> v;
            db.set(k, v);
        }
        else if (cmd == "get") {
            string k;
            cin >> k;
            cout << db.get(k) << endl;
        }
        else if (cmd == "insertOne") {
            string col;
            cin >> col;
            cin.ignore();
            string s;
            getline(cin, s);
            db.insertOne(col, json::parse(s));
        }
        else if (cmd == "find") {
            string col;
            cin >> col;
            cin.ignore();
            string s;
            getline(cin, s);
            for (auto& d : db.find(col, json::parse(s)))
                cout << d.dump() << endl;
        }
        else if (cmd == "updateOne") {
            string col;
            cin >> col;
            cin.ignore();
            string q, u;
            getline(cin, q);
            getline(cin, u);
            db.updateOne(col, json::parse(q), json::parse(u));
        }
        else if (cmd == "exit") break;
    }
}
