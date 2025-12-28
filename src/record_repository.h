#pragma once

#include <string>
#include <vector>

#include <pqxx/pqxx>

struct PlayerRecord {
    std::string name;
    int         score;
    double      play_time;  // в секундах
};

class RecordRepository {
public:
    explicit RecordRepository(const std::string& db_url);

    void InitSchema();

    void AddRecord(const std::string& name, int score, double play_time);
    std::vector<PlayerRecord> GetRecords(std::size_t start, std::size_t max_items);

private:
    std::string db_url_;
    pqxx::connection connection_{};
    void EnsureSchema();
};
