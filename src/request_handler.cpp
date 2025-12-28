#include "request_handler.h"

namespace http_handler {

    namespace json = boost::json;

    using namespace std::literals;

    json::value RequestHandler::CreateMapListJson() {
        json::array maps_array;

        for (const auto& map : game_.GetMaps()) {
            maps_array.push_back({
                {"id", *map.GetId()},
                {"name", map.GetName()}
                });
        }

        return maps_array;
    }

    json::value RequestHandler::CreateMapJson(const model::Map& map) {
        json::array roads_array;
        for (const auto& road : map.GetRoads()) {
            roads_array.push_back(CreateRoadJson(road));
        }

        json::array buildings_array;
        for (const auto& building : map.GetBuildings()) {
            buildings_array.push_back(CreateBuildingJson(building));
        }

        json::array offices_array;
        for (const auto& office : map.GetOffices()) {
            offices_array.push_back(CreateOfficeJson(office));
        }

        return {
            {"id", *map.GetId()},
            {"name", map.GetName()},
            {"roads", roads_array},
            {"buildings", buildings_array},
            {"offices", offices_array}
        };
    }

    json::value RequestHandler::CreateRoadJson(const model::Road& road) {
        auto start = road.GetStart();
        auto end = road.GetEnd();

        json::object road_obj;
        road_obj["x0"] = start.x;
        road_obj["y0"] = start.y;

        if (road.IsHorizontal()) {
            road_obj["x1"] = end.x;
        }
        else {
            road_obj["y1"] = end.y;
        }

        return road_obj;
    }


    json::value RequestHandler::CreateBuildingJson(const model::Building& building) {
        auto bounds = building.GetBounds();
        return {
            {"x", bounds.position.x},
            {"y", bounds.position.y},
            {"w", bounds.size.width},
            {"h", bounds.size.height}
        };
    }

    json::value RequestHandler::CreateOfficeJson(const model::Office& office) {
        auto pos = office.GetPosition();
        auto offset = office.GetOffset();

        return {
            {"id", *office.GetId()},
            {"x", pos.x},
            {"y", pos.y},
            {"offsetX", offset.dx},
            {"offsetY", offset.dy}
        };
    }

    json::value RequestHandler::CreateLootJson(const model::Loot& loot) {
        return {
            {"type", static_cast<int64_t>(loot.type)},
            {"pos", json::array{geom::Round6(loot.position.x), geom::Round6(loot.position.y)}}
        };
    }

    std::string RequestHandler::GetMimeType(const std::string& file_path) const {
        fs::path path(file_path);
        std::string extension = path.extension().string();


        std::transform(extension.begin(), extension.end(), extension.begin(),
            [](unsigned char c) { return std::tolower(c); });


        static const std::unordered_map<std::string, std::string> mime_types = {
            {".htm", "text/html"},
            {".html", "text/html"},
            {".css", "text/css"},
            {".txt", "text/plain"},
            {".js", "text/javascript"},
            {".json", "application/json"},
            {".xml", "application/xml"},
            {".png", "image/png"},
            {".jpg", "image/jpeg"},
            {".jpe", "image/jpeg"},
            {".jpeg", "image/jpeg"},
            {".gif", "image/gif"},
            {".bmp", "image/bmp"},
            {".ico", "image/vnd.microsoft.icon"},
            {".tiff", "image/tiff"},
            {".tif", "image/tiff"},
            {".svg", "image/svg+xml"},
            {".svgz", "image/svg+xml"},
            {".mp3", "audio/mpeg"}
        };

        auto it = mime_types.find(extension);
        if (it != mime_types.end()) {
            return it->second;
        }

        return "application/octet-stream";

    }
}  // namespace http_handler