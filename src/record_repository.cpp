#include "record_repository.h"

#include <iostream>

RecordRepository::RecordRepository(const std::string& db_url)
    : db_url_(db_url)
    , connection_{ db_url }
{
    EnsureSchema();
}

void RecordRepository::EnsureSchema() {
    pqxx::work tx{ connection_ };
    tx.exec(R"(
        CREATE TABLE IF NOT EXISTS retired_players (
            id           BIGSERIAL PRIMARY KEY,
            name         TEXT NOT NULL,
            score        INTEGER NOT NULL,
            play_time_ms BIGINT NOT NULL,
            created_at   TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        );
        CREATE INDEX IF NOT EXISTS retired_players_score_idx
            ON retired_players (score DESC, play_time_ms ASC, name ASC);
    )");
    tx.commit();
}

void RecordRepository::AddRecord(const std::string& name, int score, double play_time) {
    try {
        pqxx::work tx{ connection_ };

        // Преобразуем время игры из секунд в миллисекунды для хранения
        auto play_time_ms = static_cast<int64_t>(play_time * 1000);

        tx.exec_params(
            "INSERT INTO retired_players (name, score, play_time_ms) VALUES ($1, $2, $3)",
            name, score, play_time_ms
        );

        tx.commit();
        std::cout << "Record saved: " << name << ", score: " << score
            << ", play time: " << play_time << "s" << std::endl;
    }
    catch (const std::exception& e) {
        std::cerr << "Failed to add record: " << e.what() << std::endl;
    }
}

std::vector<PlayerRecord> RecordRepository::GetRecords(std::size_t start, std::size_t max_items) {
    std::vector<PlayerRecord> result;

    try {
        pqxx::read_transaction tx{ connection_ };

        auto res = tx.exec_params(
            "SELECT name, score, play_time_ms FROM retired_players "
            "ORDER BY score DESC, play_time_ms ASC, name ASC "
            "OFFSET $1 LIMIT $2",
            start, max_items
        );

        result.reserve(res.size());
        for (const auto& row : res) {
            PlayerRecord r;
            r.name = row["name"].c_str();
            r.score = row["score"].as<int>();
            // Преобразуем миллисекунды обратно в секунды
            r.play_time = row["play_time_ms"].as<int64_t>() / 1000.0;
            result.push_back(std::move(r));
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Failed to get records: " << e.what() << std::endl;
    }

    return result;
}


void RecordRepository::InitSchema() {
    EnsureSchema();
}