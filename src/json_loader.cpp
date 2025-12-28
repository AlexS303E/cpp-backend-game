#include "json_loader.h"
#include <fstream>
#include <boost/json.hpp>

namespace json_loader {

    namespace json = boost::json;

    // Парсирование Дороги
    model::Road ParseRoad(const json::object& road_obj) {
        if (road_obj.contains("x0") && road_obj.contains("y0")) {
            model::Position start{
                static_cast<model::Coord>(road_obj.at("x0").as_int64()),
                static_cast<model::Coord>(road_obj.at("y0").as_int64())
            };

            if (road_obj.contains("x1")) {
                return model::Road(model::Road::HORIZONTAL, start,
                    static_cast<model::Coord>(road_obj.at("x1").as_int64()));
            }
            else if (road_obj.contains("y1")) {
                return model::Road(model::Road::VERTICAL, start,
                    static_cast<model::Coord>(road_obj.at("y1").as_int64()));
            }
        }
        throw std::runtime_error("Invalid road data");
    }

    // Парсирование Офиса
    model::Office ParseOffice(const json::object& office_obj) {
        auto id = model::Office::Id(std::string(office_obj.at("id").as_string()));
        model::Position position{
            static_cast<model::Coord>(office_obj.at(X_Cord).as_int64()),
            static_cast<model::Coord>(office_obj.at(Y_Cord).as_int64())
        };
        model::Offset offset{
            static_cast<model::Dimension>(office_obj.at("offsetX").as_int64()),
            static_cast<model::Dimension>(office_obj.at("offsetY").as_int64())
        };
        return model::Office(id, position, offset);
    }

    // Парсирование Здания
    model::Building ParseBuilding(const json::object& building_obj) {
        model::Position position{
            static_cast<model::Coord>(building_obj.at(X_Cord).as_int64()),
            static_cast<model::Coord>(building_obj.at(Y_Cord).as_int64())
        };
        model::Size size{
            static_cast<model::Dimension>(building_obj.at(W_Cord).as_int64()),
            static_cast<model::Dimension>(building_obj.at(H_Cord).as_int64())
        };
        return model::Building(model::Rectangle{ position, size });
    }

    // Парсинг Карты
    void ParseMap(model::Game& game, const json::object& map_obj, double default_dog_speed, size_t default_bag_capacity) {
        auto id = model::Map::Id(std::string(map_obj.at("id").as_string()));
        auto name = std::string(map_obj.at("name").as_string());

        model::Map map(id, name);

        // Устанавливаем скорость собаки
        if (map_obj.contains("dogSpeed")) {
            map.SetDogSpeed(map_obj.at("dogSpeed").as_double());
        }
        else {
            map.SetDogSpeed(default_dog_speed);
        }

        // Устанавливаем вместимость рюкзака
        if (map_obj.contains("bagCapacity")) {
            map.SetBagCapacity(map_obj.at("bagCapacity").as_int64());
        }
        else {
            map.SetBagCapacity(default_bag_capacity);
        }

        // Дороги
        if (map_obj.contains("roads")) {
            for (const auto& road_val : map_obj.at("roads").as_array()) {
                map.AddRoad(ParseRoad(road_val.as_object()));
            }
        }

        // Дома
        if (map_obj.contains("buildings")) {
            for (const auto& building_val : map_obj.at("buildings").as_array()) {
                map.AddBuilding(ParseBuilding(building_val.as_object()));
            }
        }

        // Офисы
        if (map_obj.contains("offices")) {
            for (const auto& office_val : map_obj.at("offices").as_array()) {
                map.AddOffice(ParseOffice(office_val.as_object()));
            }
        }

        // Лут
        if (map_obj.contains("lootTypes")) {
            auto loot_types = map_obj.at("lootTypes").as_array();
            map.SetLootTypes(loot_types);
        }

        game.AddMap(std::move(map));
    }


    std::unique_ptr<model::Game> LoadGame(const std::filesystem::path& json_path) {
        try {
            // Проверяем, что путь существует и это обычный файл
            if (!std::filesystem::exists(json_path)) {
                throw std::runtime_error("File does not exist: " + json_path.string());
            }

            if (!std::filesystem::is_regular_file(json_path)) {
                throw std::runtime_error("Path is not a regular file: " + json_path.string());
            }

            // Открываем файл в бинарном режиме
            std::ifstream file(json_path, std::ios::binary | std::ios::ate);
            if (!file.is_open()) {
                throw std::runtime_error("Failed to open file: " + json_path.string());
            }

            // Получаем размер файла
            auto size = file.tellg();
            file.seekg(0, std::ios::beg);

            // Читаем содержимое
            std::string content(size, '\0');
            if (!file.read(content.data(), size)) {
                throw std::runtime_error("Failed to read file: " + json_path.string());
            }

            auto json_data = json::parse(content);
            auto& root_obj = json_data.as_object();

            // Получаем дефолтную скорость
            double default_dog_speed = 1.0; // значение по умолчанию
            if (root_obj.contains("defaultDogSpeed")) {
                default_dog_speed = root_obj.at("defaultDogSpeed").as_double();
            }

            if (!root_obj.contains("maps")) {
                throw std::runtime_error("Missing 'maps' field in JSON");
            }

            auto& maps_array = root_obj.at("maps").as_array();

            // Создаем игру в динамической памяти
            auto game = std::make_unique<model::Game>();

            // Загружаем конфигурацию генератора трофеев
            if (root_obj.contains("lootGeneratorConfig")) {
                auto& config = root_obj.at("lootGeneratorConfig").as_object();
                double base_interval = config.at("period").as_double();
                double probability = config.at("probability").as_double();
                game->SetLootGeneratorConfig(base_interval, probability);
            }

            // Получаем стандартную вместимость рюкзака
            size_t default_bag_capacity = 3; // значение по умолчанию
            if (root_obj.contains("defaultBagCapacity")) {
                default_bag_capacity = root_obj.at("defaultBagCapacity").as_int64();
            }

            double dog_retirement_time = 60.0;
            if (root_obj.contains("dogRetirementTime")) {
                dog_retirement_time = root_obj.at("dogRetirementTime").as_double();
            }
            game->SetDogRetirementTime(dog_retirement_time);

            for (const auto& map_val : maps_array) {
                ParseMap(*game, map_val.as_object(), default_dog_speed, default_bag_capacity);
            }

            return game;

        }
        catch (const std::filesystem::filesystem_error& ex) {
            throw std::runtime_error("Filesystem error: " + std::string(ex.what()));
        }
        catch (const std::exception& ex) {
            throw std::runtime_error("JSON parsing error: " + std::string(ex.what()));
        }
    }

}  // namespace json_loader