#pragma once

#include "model.h"
#include <boost/json.hpp>
#include <filesystem>
#include <fstream>

namespace state_serializer {

    class StateSerializer {
    public:
        void Serialize(const model::Game& game, const std::filesystem::path& file_path);
        void Deserialize(model::Game& game, const std::filesystem::path& file_path);

        // Методы для сериализации отдельных объектов
        boost::json::object SerializeGame(const model::Game& game);
        boost::json::object SerializeSession(const model::GameSession& session);
        boost::json::object SerializePlayer(const model::Player& player);
        boost::json::object SerializeDog(const model::Dog& dog);
        boost::json::object SerializeLoot(const geom::Loot& loot);
        std::string SerializeToken(const Token& token);

        // Методы для десериализации
        void DeserializeGame(model::Game& game, const boost::json::object& json_val);
        void DeserializeSession(model::Game& game, const boost::json::object& json_val);
        model::Player DeserializePlayer(const boost::json::object& json_val);
        model::Dog DeserializeDog(const boost::json::object& json_val);
        geom::Loot DeserializeLoot(const boost::json::object& json_val);
        Token DeserializeToken(const std::string& token_str);
    };

} // namespace state_serializer