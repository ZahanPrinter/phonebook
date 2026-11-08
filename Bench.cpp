#include <chrono>
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <random>
#include "PhoneBook.h"
using namespace std;
using clk = chrono::steady_clock;

int main() {
    // Read rows, then shuffle so BST isn't fed sorted input (also test sorted below)
    vector<vector<string>> rows;
    { ifstream f("contacts.csv"); string line;
      while (getline(f, line)) { if (!line.empty() && line.back()=='\r') line.pop_back(); if (line.empty()) continue;
        stringstream ss(line); vector<string> r(4); for (auto& x : r) getline(ss, x, ','); rows.push_back(r); } }
    cout << "rows: " << rows.size() << "\n";

    for (int mode = 0; mode < 2; ++mode) {
        vector<vector<string>> data = rows;
        if (mode == 1) { mt19937 g(42); shuffle(data.begin(), data.end(), g); }
        cout << (mode==0 ? "\n== load order: file order (sorted by name) ==\n" : "\n== load order: shuffled ==\n");
        PhoneBook book;
        auto t0 = clk::now();
        for (auto& r : data) book.addContact(r[0], r[1], r[2], r[3]);
        auto t1 = clk::now();
        cout << "load: " << chrono::duration<double, milli>(t1-t0).count() << " ms, contacts=" << book.contactCount() << "\n";

        // Hash lookups: all 50k phones, shuffled, repeated
        vector<string> phones; for (auto& r : data) phones.push_back(r[1]);
        mt19937 g(7); shuffle(phones.begin(), phones.end(), g);
        const size_t REPS = 40; size_t hits = 0;
        // warmup
        for (auto& p : phones) hits += book.findByPhone(p) != nullptr;
        double best = 1e18, sum = 0; vector<double> runs;
        for (int run = 0; run < 7; ++run) {
            auto a = clk::now();
            for (size_t k = 0; k < REPS; ++k) for (auto& p : phones) hits += book.findByPhone(p) != nullptr;
            auto b = clk::now();
            double ns = chrono::duration<double, nano>(b-a).count() / (REPS * phones.size());
            runs.push_back(ns);
        }
        sort(runs.begin(), runs.end());
        cout << "hash findByPhone (hit, random order, 50k keys): median " << runs[3] << " ns, min " << runs.front() << " ns, max " << runs.back() << " ns (hits=" << hits << ")\n";

        // Hash lookups: single repeated key (what a naive loop measures)
        { string p = phones[0]; auto a = clk::now(); size_t h=0; const size_t N=5000000; for (size_t i=0;i<N;++i) h += book.findByPhone(p)!=nullptr; auto b = clk::now();
          cout << "hash findByPhone (SAME key repeated): " << chrono::duration<double,nano>(b-a).count()/N << " ns (hits=" << h << ")\n"; }

        // Prefix search
        vector<string> prefixes = {"Aisha", "Ai", "Zo", "Dan", "Priya R"};
        for (auto& pf : prefixes) {
            size_t cnt = 0; const int N = 200;
            auto a = clk::now(); for (int i=0;i<N;++i) cnt = book.findByPrefix(pf).size(); auto b = clk::now();
            cout << "trie prefix '" << pf << "': " << chrono::duration<double,micro>(b-a).count()/N << " us/query, matches=" << cnt << "\n";
        }
        // Range query
        { size_t cnt=0; const int N=50; auto a = clk::now(); for (int i=0;i<N;++i) cnt = book.listRange("Dan","Priya").size(); auto b = clk::now();
          cout << "bst range Dan..Priya: " << chrono::duration<double,micro>(b-a).count()/N << " us/query, matches=" << cnt << "\n"; }
    }
}