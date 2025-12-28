#pragma once

#include <filesystem>
#include <memory>
#include <fstream>
#include <boost/json.hpp>

#include "model.h"

namespace json_loader {

    constexpr const char* X_Cord = "x";
    constexpr const char* Y_Cord = "y";
    constexpr const char* W_Cord = "w";
    constexpr const char* H_Cord = "h";

    model::Road ParseRoad(const boost::json::object& road_obj);

    model::Office ParseOffice(const boost::json::object& office_obj);

    model::Building ParseBuilding(const boost::json::object& building_obj);

    void ParseMap(model::Game& game, const boost::json::object& map_obj, double default_dog_speed);

    std::unique_ptr<model::Game> LoadGame(const std::filesystem::path& json_path);

    

}  // namespace json_loader