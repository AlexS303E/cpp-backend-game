#include "state_serializer.h"
#include <fstream>
#include <sstream>
#include <boost/json.hpp>
#include <iostream>

namespace state_serializer {

    namespace json = boost::json;

    void StateSerializer::Serialize(const model::Game& game, const std::filesystem::path& file_path) {
        auto game_obj = SerializeGame(game);

        // Создаем временный файл для атомарности
        auto temp_path = file_path;
        temp_path += ".tmp";

        {
            std::ofstream file(temp_path);
            if (!file) {
                throw std::runtime_error("Cannot open state file for writing: " + temp_path.string());
            }
            file << json::serialize(game_obj);
        }

        // Атомарное переименование
        std::filesystem::rename(temp_path, file_path);
    }

    void StateSerializer::Deserialize(model::Game& game, const std::filesystem::path& file_path) {
        if (!std::filesystem::exists(file_path)) {
            std::cout << "State file does not exist, starting with fresh state: " << file_path << std::endl;
            return;
        }

        std::ifstream file(file_path);
        if (!file) {
            throw std::runtime_error("Cannot open state file for reading: " + file_path.string());
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string json_str = buffer.str();

        if (json_str.empty()) {
            std::cout << "State file is empty, starting with fresh state." << std::endl;
            return;
        }

        try {
            auto value = json::parse(json_str);
            if (!value.is_object()) {
                throw std::runtime_error("Invalid state file format: expected object");
            }

            DeserializeGame(game, value.as_object());
        }
        catch (const std::exception& ex) {
            throw std::runtime_error("Failed to parse state file: " + std::string(ex.what()));
        }
    }

    boost::json::object StateSerializer::SerializeGame(const model::Game& game) {
        boost::json::object game_obj;

        // Сериализуем сессии
        json::array sessions_array;
        for (const auto& session : game.GetSessions()) {
            sessions_array.push_back(SerializeSession(session));
        }
        game_obj["sessions"] = std::move(sessions_array);

        return game_obj;
    }

    boost::json::object StateSerializer::SerializeSession(const model::GameSession& session) {
        boost::json::object session_obj;

        session_obj["id"] = *session.GetId();
        session_obj["map_id"] = *session.GetMap()->GetId();
        session_obj["next_loot_id"] = session.GetNextLootId();

        // Сериализуем игроков
        json::array players_array;
        for (const auto& player : session.GetPlayers()) {
            players_array.push_back(SerializePlayer(player));
        }
        session_obj["players"] = std::move(players_array);

        // Сериализуем лут
        json::array loot_array;
        for (const auto& loot : session.GetLoots()) {
            loot_array.push_back(SerializeLoot(loot));
        }
        session_obj["loots"] = std::move(loot_array);

        return session_obj;
    }


    boost::json::object StateSerializer::SerializePlayer(const model::Player& player) {
        boost::json::object player_obj;

        player_obj["id"] = static_cast<int64_t>(*player.GetId());
        player_obj["token"] = SerializeToken(player.GetToken());
        player_obj["score"] = player.GetScore();
        player_obj["bag_capacity"] = static_cast<int64_t>(player.GetBagCapacity());

        // Сериализуем собаку
        player_obj["dog"] = SerializeDog(player.GetDog());

        // Сериализуем рюкзак
        json::array bag_array;
        for (const auto& loot : player.GetBag()) {
            bag_array.push_back(SerializeLoot(loot));
        }
        player_obj["bag"] = std::move(bag_array);

        return player_obj;
    }

    boost::json::object StateSerializer::SerializeDog(const model::Dog& dog) {
        boost::json::object dog_obj;

        dog_obj["id"] = *dog.GetId();
        dog_obj["name"] = dog.GetName();
        dog_obj["map_id"] = *dog.GetMapId();

        // Позиция и скорость
        boost::json::object pos_obj;
        pos_obj["x"] = geom::Round6(dog.GetPosition().x);
        pos_obj["y"] = geom::Round6(dog.GetPosition().y);
        dog_obj["position"] = std::move(pos_obj);

        boost::json::object speed_obj;
        speed_obj["vx"] = geom::Round6(dog.GetSpeed().vx);
        speed_obj["vy"] = geom::Round6(dog.GetSpeed().vy);
        dog_obj["speed"] = std::move(speed_obj);

        // Направление
        switch (dog.GetDirection()) {
        case geom::Direction::NORTH: dog_obj["direction"] = "north"; break;
        case geom::Direction::SOUTH: dog_obj["direction"] = "south"; break;
        case geom::Direction::WEST: dog_obj["direction"] = "west"; break;
        case geom::Direction::EAST: dog_obj["direction"] = "east"; break;
        default: dog_obj["direction"] = "north"; break;
        }

        return dog_obj;
    }



    boost::json::object StateSerializer::SerializeLoot(const geom::Loot& loot) {
        boost::json::object loot_obj;

        loot_obj["id"] = static_cast<int64_t>(*loot.id);
        loot_obj["type"] = static_cast<int64_t>(loot.type);
        loot_obj["value"] = loot.value;

        json::object pos_obj;
        pos_obj["x"] = geom::Round6(loot.position.x);
        pos_obj["y"] = geom::Round6(loot.position.y);
        loot_obj["position"] = std::move(pos_obj);

        return loot_obj;
    }

    std::string StateSerializer::SerializeToken(const Token& token) {
        return *token;
    }

    void StateSerializer::DeserializeGame(model::Game& game, const boost::json::object& json_val) {
        if (!json_val.contains("sessions")) {
            return;
        }

        const auto& sessions_array = json_val.at("sessions").as_array();
        for (const auto& session_val : sessions_array) {
            try {
                DeserializeSession(game, session_val.as_object());
            }
            catch (const std::exception& ex) {
                std::cerr << "Failed to deserialize session: " << ex.what() << std::endl;
                // Продолжаем с другими сессиями
            }
        }
    }

    void StateSerializer::DeserializeSession(model::Game& game, const boost::json::object& json_val) {
        if (!json_val.contains("map_id")) {
            throw std::runtime_error("Session missing map_id");
        }

        std::string map_id_str = json_val.at("map_id").as_string().c_str();
        model::Map::Id map_id{ map_id_str };

        try {
            // Получаем или создаем сессию
            model::GameSession& session = game.GetOrCreateSession(map_id);

            // Восстанавливаем next_loot_id
            if (json_val.contains("next_loot_id")) {
                session.SetNextLootId(static_cast<size_t>(json_val.at("next_loot_id").as_int64()));
            }

            // Восстанавливаем игроков
            if (json_val.contains("players")) {
                const auto& players_array = json_val.at("players").as_array();
                for (const auto& player_val : players_array) {
                    try {
                        auto player = DeserializePlayer(player_val.as_object());
                        session.AddPlayer(std::move(player));
                    }
                    catch (const std::exception& ex) {
                        std::cerr << "Failed to deserialize player: " << ex.what() << std::endl;
                    }
                }
            }

            // Восстанавливаем лут
            if (json_val.contains("loots")) {
                const auto& loots_array = json_val.at("loots").as_array();
                for (const auto& loot_val : loots_array) {
                    try {
                        auto loot = DeserializeLoot(loot_val.as_object());
                        session.AddLoot(loot);
                    }
                    catch (const std::exception& ex) {
                        std::cerr << "Failed to deserialize loot: " << ex.what() << std::endl;
                    }
                }
            }
        }
        catch (const std::exception& ex) {
            throw std::runtime_error("Failed to get or create session for map " + map_id_str + ": " + ex.what());
        }
    }

    model::Player StateSerializer::DeserializePlayer(const boost::json::object& json_val) {
        if (!json_val.contains("id") || !json_val.contains("token") ||
            !json_val.contains("score") || !json_val.contains("bag_capacity") ||
            !json_val.contains("dog")) {
            throw std::runtime_error("Player missing required fields");
        }

        model::Player::Id id{ static_cast<size_t>(json_val.at("id").as_int64()) };
        std::string token_str = json_val.at("token").as_string().c_str();
        Token token = DeserializeToken(token_str); // Используем новый метод
        int score = json_val.at("score").as_int64();
        size_t bag_capacity = json_val.at("bag_capacity").as_int64();

        auto dog = DeserializeDog(json_val.at("dog").as_object());

        model::Player player(id, std::move(dog), std::move(token), bag_capacity);
        player.AddScore(score);

        // Восстанавливаем рюкзак
        if (json_val.contains("bag")) {
            const auto& bag_array = json_val.at("bag").as_array();
            for (const auto& loot_val : bag_array) {
                try {
                    auto loot = DeserializeLoot(loot_val.as_object());
                    player.AddToBag(loot);
                }
                catch (const std::exception& ex) {
                    std::cerr << "Failed to deserialize loot in bag: " << ex.what() << std::endl;
                }
            }
        }

        return player;
    }

    model::Dog StateSerializer::DeserializeDog(const boost::json::object& json_val) {
        if (!json_val.contains("id") || !json_val.contains("name") ||
            !json_val.contains("map_id") || !json_val.contains("position") ||
            !json_val.contains("speed") || !json_val.contains("direction")) {
            throw std::runtime_error("Dog missing required fields");
        }

        model::Dog::Id id{ json_val.at("id").as_string().c_str() };
        std::string name = json_val.at("name").as_string().c_str();
        model::Map::Id map_id{ json_val.at("map_id").as_string().c_str() };

        model::Dog dog(id, name, map_id);

        // Восстанавливаем позицию
        const auto& pos_obj = json_val.at("position").as_object();
        geom::Position pos{ pos_obj.at("x").as_double(), pos_obj.at("y").as_double() };
        dog.SetPosition(pos);

        // Восстанавливаем скорость
        const auto& speed_obj = json_val.at("speed").as_object();
        geom::Speed speed{ speed_obj.at("vx").as_double(), speed_obj.at("vy").as_double() };
        dog.SetSpeed(speed);

        // Восстанавливаем направление
        std::string dir_str = json_val.at("direction").as_string().c_str();
        geom::Direction direction;
        if (dir_str == "north") direction = geom::Direction::NORTH;
        else if (dir_str == "south") direction = geom::Direction::SOUTH;
        else if (dir_str == "west") direction = geom::Direction::WEST;
        else if (dir_str == "east") direction = geom::Direction::EAST;
        else {
            std::cerr << "Invalid direction: " << dir_str << ", defaulting to north" << std::endl;
            direction = geom::Direction::NORTH;
        }
        dog.SetDirection(direction);

        return dog;
    }

    geom::Loot StateSerializer::DeserializeLoot(const boost::json::object& json_val) {
        if (!json_val.contains("id") || !json_val.contains("type") ||
            !json_val.contains("value") || !json_val.contains("position")) {
            throw std::runtime_error("Loot missing required fields");
        }

        geom::Loot::Id id{ static_cast<size_t>(json_val.at("id").as_int64()) };
        size_t type = json_val.at("type").as_int64();
        int value = json_val.at("value").as_int64();

        const auto& pos_obj = json_val.at("position").as_object();
        geom::Position pos{ pos_obj.at("x").as_double(), pos_obj.at("y").as_double() };

        return geom::Loot(id, type, pos, value);
    }

    Token StateSerializer::DeserializeToken(const std::string& token_str) {
        return Token{ token_str };
    }

} // namespace state_serializer