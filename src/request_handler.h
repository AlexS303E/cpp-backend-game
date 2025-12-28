#pragma once
#include "http_server.h"
#include "model.h"
#include "token.h"
#include "application_listener.h"
#include "record_repository.h"

#include <boost/asio/strand.hpp>
#include <boost/beast/http.hpp>
#include <boost/json.hpp>
#include <chrono>
#include <iostream>
#include <filesystem>
#include <fstream>
#include <unordered_map>
#include <algorithm>

namespace http_handler {
    namespace beast = boost::beast;
    namespace http = beast::http;
    namespace json = boost::json;
    namespace net = boost::asio;
    namespace fs = std::filesystem;

    using StringRequest = http::request<http::string_body>;
    using StringResponse = http::response<http::string_body>;

    class RequestHandler : public std::enable_shared_from_this<RequestHandler> {
    public:
        using Strand = net::strand<net::io_context::executor_type>;

        RequestHandler(model::Game& game, Strand api_strand,
            std::string www_root, bool manual_tick_enabled,
            bool randomize_spawn_points,
            app::ApplicationListener* tick_listener,
            std::shared_ptr<RecordRepository> record_repo)
            : game_(game)
            , api_strand_(api_strand)
            , static_path_(std::move(www_root))
            , manual_tick_enabled_(manual_tick_enabled)
            , randomize_spawn_points_(randomize_spawn_points)
            , tick_listener_(tick_listener)
            , record_repo_(std::move(record_repo)) {
        }

        RequestHandler(const RequestHandler&) = delete;
        RequestHandler& operator=(const RequestHandler&) = delete;

        template <typename Body, typename Allocator, typename Send>
        void operator()(http::request<Body, http::basic_fields<Allocator>>&& req, Send&& send) {
            auto version = req.version();
            auto keep_alive = req.keep_alive();

            try {
                const auto target = std::string_view(req.target());

                // API endpoints обрабатываем в strand
                if (target.starts_with("/api/")) {
                    // Создаем копию запроса для лямбды
                    auto req_copy = std::make_shared<http::request<Body, http::basic_fields<Allocator>>>(std::move(req));

                    auto handle = [self = shared_from_this(), send = std::forward<Send>(send),
                        req_copy, version, keep_alive]() mutable {
                        try {
                            // Этот код выполняется внутри strand
                            auto response = self->HandleApiRequest(*req_copy);
                            return send(std::move(response));
                        }
                        catch (const std::exception& e) {
                            auto error_response = self->MakeErrorResponse(
                                *req_copy, http::status::internal_server_error,
                                "Internal server error", "internalError");
                            return send(std::move(error_response));
                        }
                        };
                    return net::dispatch(api_strand_, std::move(handle));
                }

                // Статические файлы обрабатываем как раньше
                auto response = HandleNonApiRequest(std::move(req));
                return send(std::move(response));

            }
            catch (const std::exception& e) {
                auto error_response = MakeErrorResponse(
                    req, http::status::internal_server_error,
                    "Internal server error", "internalError");
                return send(std::move(error_response));
            }
        }

        template <typename Body, typename Allocator>
        StringResponse HandleGameTick(const http::request<Body, http::basic_fields<Allocator>>& req) {
            if (req.method() != http::verb::post) {
                return MakeMethodNotAllowedResponse(req, { "POST" });
            }

            auto content_type = req.find(http::field::content_type);
            if (content_type == req.end() || content_type->value() != "application/json") {
                return MakeErrorResponse(req, http::status::bad_request,
                    "Invalid content type", "invalidArgument");
            }

            try {
                auto json_body = json::parse(req.body());
                auto& obj = json_body.as_object();

                if (!obj.contains("timeDelta")) {
                    return MakeErrorResponse(req, http::status::bad_request,
                        "Missing timeDelta field", "invalidArgument");
                }

                auto time_delta_val = obj.at("timeDelta");
                if (!time_delta_val.is_int64()) {
                    return MakeErrorResponse(req, http::status::bad_request,
                        "Invalid timeDelta value", "invalidArgument");
                }

                // timeDelta is in **milliseconds**
                auto time_delta_ms = time_delta_val.as_int64();
                if (time_delta_ms < 0) {
                    return MakeErrorResponse(req, http::status::bad_request,
                        "Invalid timeDelta value", "invalidArgument");
                }

                // Convert milliseconds -> seconds for game logic
                double delta_time = static_cast<double>(time_delta_ms) / 1000.0;

                // Convert milliseconds -> microseconds for Game::SetTickPeriod
                game_.SetTickPeriod(time_delta_ms * 1000);

                // Advance game state
                game_.UpdateState(delta_time);

                // Notify listener in milliseconds
                if (tick_listener_) {
                    auto delta_ms = std::chrono::milliseconds(time_delta_ms);
                    tick_listener_->OnTick(delta_ms);
                }

                json::value response_json = json::object{};
                auto response = MakeJsonResponse(req, http::status::ok, json::serialize(response_json));
                response.set(http::field::cache_control, "no-cache");
                return response;
            }
            catch (const std::exception& e) {
                return MakeErrorResponse(req, http::status::bad_request,
                    "Failed to parse tick request JSON", "invalidArgument");
            }
        }


    private:
        model::Game& game_;
        Strand api_strand_;
        TokenGenerator token_generator_;
        size_t next_player_id_ = 0;
        fs::path static_path_ = "static";
        bool manual_tick_enabled_;
        bool randomize_spawn_points_;
        app::ApplicationListener* tick_listener_ = nullptr;
        std::shared_ptr<RecordRepository> record_repo_;

        std::string GetMimeType(const std::string& file_path) const;
        json::value CreateLootJson(const model::Loot& loot);


        template <typename Body, typename Allocator>
        StringResponse HandleGetRecords(const http::request<Body, http::basic_fields<Allocator>>& req) {
            if (!record_repo_) {
                return MakeErrorResponse(
                    req, http::status::internal_server_error,
                    "Records storage is not configured", "internalError");
            }

            // Параметры по умолчанию
            std::size_t start = 0;
            std::size_t max_items = 100;

            // Парсим query
            const auto target = std::string_view(req.target());
            auto params = ParseQuery(target);

            // start
            if (auto it = params.find("start"); it != params.end()) {
                try {
                    long val = std::stol(it->second);
                    if (val < 0) {
                        return MakeErrorResponse(
                            req, http::status::bad_request,
                            "start must be non-negative", "invalidArgument");
                    }
                    start = static_cast<std::size_t>(val);
                }
                catch (...) {
                    return MakeErrorResponse(
                        req, http::status::bad_request,
                        "Invalid start parameter", "invalidArgument");
                }
            }

            // maxItems
            if (auto it = params.find("maxItems"); it != params.end()) {
                try {
                    long val = std::stol(it->second);
                    if (val <= 0) {
                        return MakeErrorResponse(
                            req, http::status::bad_request,
                            "maxItems must be positive", "invalidArgument");
                    }
                    if (val > 100) {
                        return MakeErrorResponse(
                            req, http::status::bad_request,
                            "maxItems must not exceed 100", "invalidArgument");
                    }
                    max_items = static_cast<std::size_t>(val);
                }
                catch (...) {
                    return MakeErrorResponse(
                        req, http::status::bad_request,
                        "Invalid maxItems parameter", "invalidArgument");
                }
            }

            // Читаем данные из БД
            std::vector<PlayerRecord> records;
            try {
                records = record_repo_->GetRecords(start, max_items);
            }
            catch (const std::exception& e) {
                return MakeErrorResponse(
                    req, http::status::internal_server_error,
                    "Failed to fetch records", "internalError");
            }

            // Формируем JSON-массив
            json::array arr;
            arr.reserve(records.size());

            for (const auto& r : records) {
                json::object o;
                o["name"] = r.name;
                o["score"] = r.score;
                o["playTime"] = r.play_time;
                arr.push_back(std::move(o));
            }

            auto body = json::serialize(arr);
            auto response = MakeJsonResponse(req, http::status::ok, body);
            response.set(http::field::cache_control, "no-cache");
            // Content-Type и Content-Length выставит MakeJsonResponse/prepare_payload

            return response;
        }



        std::unordered_map<std::string, std::string> ParseQuery(std::string_view target) const {
            std::unordered_map<std::string, std::string> params;

            auto qpos = target.find('?');
            if (qpos == std::string_view::npos) {
                return params;
            }

            auto query = target.substr(qpos + 1);

            while (!query.empty()) {
                auto amp = query.find('&');
                auto part = query.substr(0, amp);

                auto eq = part.find('=');
                if (eq != std::string_view::npos) {
                    std::string key(part.substr(0, eq));
                    std::string val(part.substr(eq + 1));
                    params.emplace(std::move(key), std::move(val));
                }

                if (amp == std::string_view::npos) {
                    break;
                }
                query.remove_prefix(amp + 1);
            }

            return params;
        }


        template <typename Body, typename Allocator>
        StringResponse HandleFileRequest(const http::request<Body, http::basic_fields<Allocator>>& req, const std::string& file_path) const {
            try {
                auto full_path = static_path_ / file_path;

                // Проверяем существование файла
                if (!fs::exists(full_path) || !fs::is_regular_file(full_path)) {
                    return MakeErrorResponse(
                        req, http::status::not_found,
                        "File not found", "fileNotFound");
                }

                // Открываем файл
                std::ifstream file(full_path, std::ios::binary);
                if (!file) {
                    return MakeErrorResponse(
                        req, http::status::internal_server_error,
                        "Cannot open file", "fileError");
                }

                // Читаем содержимое файла
                std::string content((std::istreambuf_iterator<char>(file)),
                    std::istreambuf_iterator<char>());

                // Определяем MIME-тип
                std::string mime_type = GetMimeType(file_path);

                // Создаем ответ
                StringResponse response;
                response.result(http::status::ok);
                response.version(req.version());
                response.set(http::field::content_type, mime_type);
                response.set(http::field::cache_control, "max-age=3600"); // Кэширование на 1 час
                response.body() = content;
                response.prepare_payload();
                response.keep_alive(req.keep_alive());

                return response;

            }
            catch (...) {
                return MakeErrorResponse(
                    req, http::status::internal_server_error,
                    "File reading error", "fileError");
            }
        }

        template <typename Body, typename Allocator>
        StringResponse HandlePlayerAction(const http::request<Body, http::basic_fields<Allocator>>& req) {
            // Проверяем метод запроса
            if (req.method() != http::verb::post) {
                return MakeMethodNotAllowedResponse(req, { "POST" });
            }

            // Проверяем заголовок Authorization
            auto auth_header = req.find(http::field::authorization);
            if (auth_header == req.end()) {
                return MakeInvalidTokenResponse(req, "Authorization header is required");
            }

            auto auth_value = std::string(auth_header->value());
            if (auth_value.length() < 7 || !auth_value.starts_with("Bearer ")) {
                return MakeInvalidTokenResponse(req, "Invalid authorization format");
            }

            auto token_str = auth_value.substr(7);
            if (token_str.length() != 32 || !std::all_of(token_str.begin(), token_str.end(), [](char c) {
                return std::isxdigit(static_cast<unsigned char>(c));
                })) {
                return MakeInvalidTokenResponse(req, "Invalid token format");
            }

            Token token{ token_str };

            // Находим игрока по токену
            auto player = game_.FindPlayerByToken(token);
            if (!player) {
                return MakeUnknownTokenResponse(req);
            }

            // Проверяем Content-Type
            auto content_type = req.find(http::field::content_type);
            if (content_type == req.end() || content_type->value() != "application/json") {
                return MakeErrorResponse(req, http::status::bad_request,
                    "Invalid content type", "invalidArgument");
            }

            try {
                auto json_body = json::parse(req.body());
                auto& obj = json_body.as_object();

                if (!obj.contains("move")) {
                    return MakeErrorResponse(req, http::status::bad_request,
                        "Missing move field", "invalidArgument");
                }

                auto move_val = obj.at("move");
                if (!move_val.is_string()) {
                    return MakeErrorResponse(req, http::status::bad_request,
                        "Invalid move value", "invalidArgument");
                }

                auto move_str = std::string(move_val.as_string());

                // Обновляем направление и скорость собаки
                auto& dog = player->GetDog();

                // Получаем скорость из карты
                auto map_id = dog.GetMapId();
                auto map = game_.FindMap(map_id);
                if (!map) {
                    return MakeErrorResponse(req, http::status::internal_server_error,
                        "Map not found", "internalError");
                }

                double speed = map->GetDogSpeed();

                // Устанавливаем скорость в зависимости от направления
                if (move_str == "L") {
                    dog.SetDirection(model::Direction::WEST);
                    dog.SetSpeed(model::Speed{ -speed, 0.0 });
                }
                else if (move_str == "R") {
                    dog.SetDirection(model::Direction::EAST);
                    dog.SetSpeed(model::Speed{ speed, 0.0 });
                }
                else if (move_str == "U") {
                    dog.SetDirection(model::Direction::NORTH);
                    dog.SetSpeed(model::Speed{ 0.0, -speed });
                }
                else if (move_str == "D") {
                    dog.SetDirection(model::Direction::SOUTH);
                    dog.SetSpeed(model::Speed{ 0.0, speed });
                }
                else if (move_str == "") {
                    // Остановка
                    dog.SetSpeed(model::Speed{ 0.0, 0.0 });
                }
                else {
                    return MakeErrorResponse(req, http::status::bad_request,
                        "Invalid move direction", "invalidArgument");
                }


                json::value response_json = json::object{};
                auto response = MakeJsonResponse(req, http::status::ok, json::serialize(response_json));
                response.set(http::field::cache_control, "no-cache");
                return response;

            }
            catch (...) {
                return MakeErrorResponse(req, http::status::bad_request,
                    "Failed to parse player action JSON", "invalidArgument");
            }
        }

        template <typename Body, typename Allocator>
        StringResponse HandleApiRequest(const http::request<Body, http::basic_fields<Allocator>>& req) {
            const auto target = std::string_view(req.target());
            const auto method = req.method();

            auto path = target.substr(0, target.find('?'));



            // POST /api/v1/game/join
            if (target == "/api/v1/game/join") {
                if (method == http::verb::post) {
                    return HandleJoinGame(req);
                }
                return MakeMethodNotAllowedResponse(req, { "POST" });
            }
            // GET /api/v1/game/players
            else if (target == "/api/v1/game/players") {
                if (method == http::verb::get || method == http::verb::head) {
                    return HandleGetPlayers(req);
                }
                return MakeMethodNotAllowedResponse(req, { "GET", "HEAD" });
            }
            // GET /api/v1/game/state
            else if (target == "/api/v1/game/state") {
                if (method == http::verb::get || method == http::verb::head) {
                    return HandleGetGameState(req);
                }
                return MakeMethodNotAllowedResponse(req, { "GET", "HEAD" });
            }
            // POST /api/v1/game/tick
            else if (target == "/api/v1/game/tick") {
                if (method == http::verb::post) {
                    return HandleGameTick(req);
                }
                return MakeMethodNotAllowedResponse(req, { "POST" });
            }
            // POST /api/v1/game/player/action
            else if (target == "/api/v1/game/player/action") {
                if (method == http::verb::post) {
                    return HandlePlayerAction(req);
                }
                return MakeMethodNotAllowedResponse(req, { "POST" });
            }
            // GET /api/v1/maps
            else if (target == "/api/v1/maps") {
                if (method == http::verb::get || method == http::verb::head) {
                    return HandleGetMaps(req);
                }
                return MakeMethodNotAllowedResponse(req, { "GET", "HEAD" });
            }
            // GET /api/v1/maps/{id}
            else if (target.starts_with("/api/v1/maps/")) {
                if (method == http::verb::get || method == http::verb::head) {
                    return HandleGetMap(req);
                }
                return MakeMethodNotAllowedResponse(req, { "GET", "HEAD" });
            }
            else if (path == "/api/v1/game/records") {
                if (method == http::verb::get || method == http::verb::head) {
                    return HandleGetRecords(req);
                }
                return MakeMethodNotAllowedResponse(req, { "GET", "HEAD" });
            }
            return MakeErrorResponse(req, http::status::bad_request, "Invalid request", "badRequest");
        }

        template <typename Body, typename Allocator>
        StringResponse HandleNonApiRequest(http::request<Body, http::basic_fields<Allocator>>&& req) {
            return HandleStaticRequest(req);
        }

        template <typename Body, typename Allocator>
        StringResponse HandleStaticRequest(const http::request<Body, http::basic_fields<Allocator>>& req) {
            const auto target = std::string_view(req.target());

            // Если запрос корневой, возвращаем index.html
            if (target == "/" || target == "/index.html") {
                return HandleFileRequest(req, "index.html");
            }

            // Убираем начальный слэш
            std::string file_path(target.substr(1)); // убираем первый '/'

            // Защита от path traversal атак
            if (file_path.find("..") != std::string::npos) {
                return MakeErrorResponse(
                    req, http::status::bad_request,
                    "Invalid path", "invalidPath");
            }

            return HandleFileRequest(req, file_path);
        }

        template <typename Body, typename Allocator>
        StringResponse HandleJoinGame(const http::request<Body, http::basic_fields<Allocator>>& req) {
            // Проверяем Content-Type
            auto content_type = req.find(http::field::content_type);
            if (content_type == req.end() || content_type->value() != "application/json") {
                return MakeErrorResponse(req, http::status::bad_request,
                    "Invalid content type", "invalidArgument");
            }

            try {
                // Парсим JSON
                auto json_body = json::parse(req.body());
                auto& obj = json_body.as_object();

                // Проверяем обязательные поля
                if (!obj.contains("userName") || !obj.contains("mapId")) {
                    return MakeErrorResponse(req, http::status::bad_request,
                        "Missing required fields", "invalidArgument");
                }

                auto user_name = std::string(obj.at("userName").as_string());
                auto map_id = std::string(obj.at("mapId").as_string());

                // Валидация имени
                if (user_name.empty()) {
                    return MakeErrorResponse(req, http::status::bad_request,
                        "Invalid name", "invalidArgument");
                }

                // Находим карту
                auto map = game_.FindMap(model::Map::Id(map_id));
                if (!map) {
                    return MakeErrorResponse(req, http::status::not_found,
                        "Map not found", "mapNotFound");
                }

                // Создаем игрока
                auto dog_id = model::Dog::Id{ user_name + "_" + map_id };
                model::Dog dog(std::move(dog_id), user_name, model::Map::Id(map_id));

                // Устанавливаем позицию спауна
                model::Position start_position;
                if (randomize_spawn_points_) {
                    start_position = map->GetRandomPosition();
                }
                else {
                    start_position = map->GetStartPosition();
                }
                dog.SetPosition(start_position);


                // Добавляем в сессию
                auto& session = game_.GetOrCreateSession(model::Map::Id(map_id));

                size_t bag_capacity = map->GetBagCapacity();
                auto token = token_generator_.GenerateToken();
                auto player_id = model::Player::Id{ next_player_id_++ };
                model::Player player(player_id, std::move(dog), token, bag_capacity);

                session.AddPlayer(std::move(player));

                // Формируем ответ
                json::value response_json = {
                    {"authToken", *token},
                    {"playerId", static_cast<int64_t>(*player_id)}
                };

                auto response = MakeJsonResponse(req, http::status::ok, json::serialize(response_json));
                response.set(http::field::cache_control, "no-cache");
                return response;

            }
            catch (const std::exception& e) {
                return MakeErrorResponse(req, http::status::bad_request,
                    "Join game request parse error", "invalidArgument");
            }
        }

        template <typename Body, typename Allocator>
        StringResponse HandleGetPlayers(const http::request<Body, http::basic_fields<Allocator>>& req) {
            // Проверяем метод запроса
            if (req.method() != http::verb::get && req.method() != http::verb::head) {
                return MakeMethodNotAllowedResponse(req, { "GET", "HEAD" });
            }

            // Проверяем заголовок Authorization
            auto auth_header = req.find(http::field::authorization);
            if (auth_header == req.end()) {
                return MakeInvalidTokenResponse(req, "Authorization header is required");
            }

            auto auth_value = std::string(auth_header->value());

            // Проверяем формат Bearer токена
            if (auth_value.length() < 7 || !auth_value.starts_with("Bearer ")) {
                return MakeInvalidTokenResponse(req, "Invalid authorization format");
            }

            auto token_str = auth_value.substr(7);

            // Проверяем длину токена (должен быть 32 символа)
            if (token_str.length() != 32) {
                return MakeInvalidTokenResponse(req, "Invalid token length");
            }

            // Проверяем, что токен состоит только из hex-символов
            if (!std::all_of(token_str.begin(), token_str.end(), [](char c) {
                return std::isxdigit(static_cast<unsigned char>(c));
                })) {
                return MakeInvalidTokenResponse(req, "Invalid token format");
            }

            Token token{ token_str };

            // Находим игрока по токену
            auto player = game_.FindPlayerByToken(token);
            if (!player) {
                return MakeUnknownTokenResponse(req);
            }

            // Находим сессию игрока
            auto session = game_.FindSessionByMapId(player->GetDog().GetMapId());
            if (!session) {
                return MakeUnknownTokenResponse(req);
            }

            // Формируем список игроков
            json::object players_json;
            for (const auto& session_player : session->GetPlayers()) {
                players_json[std::to_string(static_cast<int64_t>(*session_player.GetId()))] = {
                    {"name", session_player.GetDog().GetName()}
                };
            }

            auto response = MakeJsonResponse(req, http::status::ok, json::serialize(players_json));
            response.set(http::field::cache_control, "no-cache");
            return response;
        }

        StringResponse HandleGetMaps(const StringRequest& req) {
            auto maps_json = CreateMapListJson();
            auto response = MakeJsonResponse(req, http::status::ok,
                req.method() == http::verb::head ? "" : json::serialize(maps_json));
            response.set(http::field::cache_control, "no-cache");
            return response;
        }

        template <typename Body, typename Allocator>
        StringResponse HandleGetMap(const http::request<Body, http::basic_fields<Allocator>>& req) {
            const auto target = std::string_view(req.target());
            auto map_id_str = target.substr(std::string("/api/v1/maps/").size());
            std::string map_id(map_id_str.begin(), map_id_str.end());

            if (map_id.empty()) {
                return MakeErrorResponse(req, http::status::bad_request, "Invalid map ID", "badRequest");
            }

            if (auto map = game_.FindMap(model::Map::Id(map_id)); map) {
                auto map_json = CreateMapJson(*map);

                if (auto loot_types = game_.GetMapLootTypes(map->GetId()); loot_types) {
                    map_json.as_object()["lootTypes"] = *loot_types;
                }
                else {
                    map_json.as_object()["lootTypes"] = json::array();
                }

                auto response = MakeJsonResponse(req, http::status::ok,
                    req.method() == http::verb::head ? "" : json::serialize(map_json));
                response.set(http::field::cache_control, "no-cache");
                return response;
            }
            else {
                return MakeErrorResponse(req, http::status::not_found, "Map not found", "mapNotFound");
            }
        }

        template <typename Body, typename Allocator>
        StringResponse HandleGetGameState(const http::request<Body, http::basic_fields<Allocator>>& req) {
            // Проверяем метод запроса
            if (req.method() != http::verb::get && req.method() != http::verb::head) {
                return MakeMethodNotAllowedResponse(req, { "GET", "HEAD" });
            }

            // Проверяем заголовок Authorization
            auto auth_header = req.find(http::field::authorization);
            if (auth_header == req.end()) {
                return MakeInvalidTokenResponse(req, "Authorization header is required");
            }

            auto auth_value = std::string(auth_header->value());

            // Проверяем формат Bearer токена
            if (auth_value.length() < 7 || !auth_value.starts_with("Bearer ")) {
                return MakeInvalidTokenResponse(req, "Invalid authorization format");
            }

            auto token_str = auth_value.substr(7);

            // Проверяем длину токена (должен быть 32 символа)
            if (token_str.length() != 32) {
                return MakeInvalidTokenResponse(req, "Invalid token length");
            }

            // Проверяем, что токен состоит только из hex-символов
            if (!std::all_of(token_str.begin(), token_str.end(), [](char c) {
                return std::isxdigit(static_cast<unsigned char>(c));
                })) {
                return MakeInvalidTokenResponse(req, "Invalid token format");
            }

            Token token{ token_str };

            // Находим игрока по токену
            auto player = game_.FindPlayerByToken(token);
            if (!player) {
                return MakeUnknownTokenResponse(req);
            }

            // Находим сессию игрока
            auto session = game_.FindSessionByMapId(player->GetDog().GetMapId());
            if (!session) {
                return MakeUnknownTokenResponse(req);
            }

            // Формируем состояние игры
            json::object players_json;
            for (const auto& session_player : session->GetPlayers()) {
                const auto& dog = session_player.GetDog();
                const auto& position = dog.GetPosition();
                const auto& speed = dog.GetSpeed();
                auto direction = dog.GetDirection();

                // Конвертируем Direction в строковое представление
                std::string dir_str;
                switch (direction) {
                case model::Direction::WEST:  dir_str = "L"; break;
                case model::Direction::EAST:  dir_str = "R"; break;
                case model::Direction::NORTH: dir_str = "U"; break;
                case model::Direction::SOUTH: dir_str = "D"; break;
                default: dir_str = "U";
                }

                // Формируем содержимое рюкзака
                json::array bag_array;
                for (const auto& loot : session_player.GetBag()) {
                    bag_array.push_back({
                        {"id", static_cast<int64_t>(*loot.id)},
                        {"type", static_cast<int64_t>(loot.type)}
                        });
                }

                players_json[std::to_string(static_cast<int64_t>(*session_player.GetId()))] = {
                    {"pos", json::array{geom::Round6(position.x), geom::Round6(position.y)}},
                    {"speed", json::array{geom::Round6(speed.vx), geom::Round6(speed.vy)}},
                    {"dir", dir_str},
                    {"bag", bag_array},
                    {"score", session_player.GetScore()}
                };
            }

            json::object lost_objects_json;
            for (const auto& loot : session->GetLoots()) {
                lost_objects_json[std::to_string(static_cast<int64_t>(*loot.id))] =
                    CreateLootJson(loot);
            }

            json::object state_json = {
                {"players", players_json},
                { "lostObjects", lost_objects_json }
            };

            auto response = MakeJsonResponse(req, http::status::ok,
                req.method() == http::verb::head ? "" : json::serialize(state_json));
            response.set(http::field::cache_control, "no-cache");
            return response;
        }

        template <typename Body, typename Allocator>
        StringResponse MakeInvalidTokenResponse(
            const http::request<Body, http::basic_fields<Allocator>>& req,
            std::string_view message) {

            json::value error_json = {
                {"code", "invalidToken"},
                {"message", std::string(message)}
            };

            StringResponse response;
            response.result(http::status::unauthorized);
            response.version(req.version());
            response.set(http::field::content_type, "application/json");
            response.set(http::field::cache_control, "no-cache");
            response.body() = json::serialize(error_json);
            response.prepare_payload();
            response.keep_alive(req.keep_alive());
            return response;
        }

        // Вспомогательные методы для создания JSON
        json::value CreateMapListJson();
        json::value CreateMapJson(const model::Map& map);
        json::value CreateRoadJson(const model::Road& road);
        json::value CreateBuildingJson(const model::Building& building);
        json::value CreateOfficeJson(const model::Office& office);






        // Вспомогательные методы для создания ответов
        template <typename Body, typename Allocator>
        StringResponse MakeUnauthorizedResponse(
            const http::request<Body, http::basic_fields<Allocator>>& req,
            std::string_view message) {

            json::value error_json = {
                {"code", "invalidToken"},
                {"message", std::string(message)}
            };

            StringResponse response;
            response.result(http::status::unauthorized);
            response.version(req.version());
            response.set(http::field::content_type, "application/json");
            response.set(http::field::cache_control, "no-cache");
            response.body() = json::serialize(error_json);
            response.prepare_payload();
            response.keep_alive(req.keep_alive());
            return response;
        }

        template <typename Body, typename Allocator>
        StringResponse MakeUnknownTokenResponse(
            const http::request<Body, http::basic_fields<Allocator>>& req) {

            json::value error_json = {
                {"code", "unknownToken"},
                {"message", "Player token has not been found"}
            };

            StringResponse response;
            response.result(http::status::unauthorized);
            response.version(req.version());
            response.set(http::field::content_type, "application/json");
            response.set(http::field::cache_control, "no-cache");
            response.body() = json::serialize(error_json);
            response.prepare_payload();
            response.keep_alive(req.keep_alive());
            return response;
        }

        template <typename Body, typename Allocator>
        StringResponse MakeMethodNotAllowedResponse(
            const http::request<Body, http::basic_fields<Allocator>>& req,
            std::initializer_list<std::string> allowed_methods) {

            json::value error_json = {
                {"code", "invalidMethod"},
                {"message", "Invalid method"}
            };

            StringResponse response;
            response.result(http::status::method_not_allowed);
            response.version(req.version());
            response.set(http::field::content_type, "application/json");
            response.set(http::field::cache_control, "no-cache");

            std::string allow_header;
            for (const auto& method : allowed_methods) {
                if (!allow_header.empty()) allow_header += ", ";
                allow_header += method;
            }
            response.set(http::field::allow, allow_header);

            response.body() = json::serialize(error_json);
            response.prepare_payload();
            response.keep_alive(req.keep_alive());
            return response;
        }

        template <typename Body, typename Allocator>
        StringResponse MakePlainTextResponse(
            const http::request<Body, http::basic_fields<Allocator>>& req,
            http::status status,
            std::string_view message) {

            StringResponse response;
            response.result(status);
            response.version(req.version());
            response.set(http::field::content_type, "text/plain");
            response.body() = std::string(message);
            response.prepare_payload();
            response.keep_alive(req.keep_alive());
            return response;
        }

        template <typename Body, typename Allocator>
        StringResponse MakeJsonResponse(
            const http::request<Body, http::basic_fields<Allocator>>& req,
            http::status status,
            std::string_view body) {

            StringResponse response;
            response.result(status);
            response.version(req.version());
            response.set(http::field::content_type, "application/json");


            if (req.method() != http::verb::head) {
                response.body() = std::string(body);
            }

            response.prepare_payload();
            response.keep_alive(req.keep_alive());
            return response;
        }

        template <typename Body, typename Allocator>
        StringResponse MakeErrorResponse(const http::request<Body, http::basic_fields<Allocator>>& req,
            http::status status, std::string_view message, std::string_view error_code) const {

            json::value error_json = {
                {"code", std::string(error_code)},
                {"message", std::string(message)}
            };

            StringResponse response;
            response.result(status);
            response.version(req.version());
            response.set(http::field::content_type, "application/json");
            response.set(http::field::cache_control, "no-cache");
            response.body() = json::serialize(error_json);
            response.prepare_payload();
            response.keep_alive(req.keep_alive());
            return response;
        }

        


    };
}  // namespace http_handler