#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <random>
#include <cmath>
#include <atomic>
#include <thread>
#include <chrono>
#include <iostream>
#include <boost/json.hpp>
#include <compare>

#include "tagged.h"
#include "token.h"
#include "loot_generator.h"
#include "collision_detector.h"

namespace model {

    static std::random_device random_device;
    static std::mt19937 random_engine{ random_device() };

    using namespace geom;

    

    class Road {
        struct HorizontalTag {
            explicit HorizontalTag() = default;
        };

        struct VerticalTag {
            explicit VerticalTag() = default;
        };

    public:
        constexpr static HorizontalTag HORIZONTAL{};
        constexpr static VerticalTag VERTICAL{};

        Road(HorizontalTag, Position start, Coord end_x) noexcept
            : start_{ start }
            , end_{ end_x, start.y } {
        }

        Road(VerticalTag, Position start, Coord end_y) noexcept
            : start_{ start }
            , end_{ start.x, end_y } {
        }

        bool IsHorizontal() const noexcept {
            return start_.y == end_.y;
        }

        bool IsVertical() const noexcept {
            return start_.x == end_.x;
        }

        Position GetStart() const noexcept {
            return start_;
        }

        Position GetEnd() const noexcept {
            return end_;
        }

        double GetMinX() const {
            return static_cast<double>(std::min(start_.x, end_.x));
        }

        double GetMaxX() const {
            return static_cast<double>(std::max(start_.x, end_.x));
        }

        double GetMinY() const {
            return static_cast<double>(std::min(start_.y, end_.y));
        }

        double GetMaxY() const {
            return static_cast<double>(std::max(start_.y, end_.y));
        }

        double GetWidth() const {
            return width_;
        }

        bool IsPositionInRoad(Position pos) const;

    private:
        Position start_;
        Position end_;
        const double width_ = 0.4;

        std::pair<Position, Position> GetBorders() const;
    };
    

    double CalculateDistanceToRoad(geom::Position pos, const Road& road);

    class Building {
    public:
        explicit Building(geom::Rectangle bounds) noexcept
            : bounds_{ bounds } {
        }

        const geom::Rectangle& GetBounds() const noexcept {
            return bounds_;
        }

    private:
        geom::Rectangle bounds_;
    };

    class Office {
    public:
        using Id = util::Tagged<std::string, Office>;

        Office(Id id, Position position, Offset offset) noexcept
            : id_{ std::move(id) }
            , position_{ position }
            , offset_{ offset } {
        }

        const Id& GetId() const noexcept {
            return id_;
        }

        Position GetPosition() const noexcept {
            return position_;
        }

        Offset GetOffset() const noexcept {
            return offset_;
        }

    private:
        Id id_;
        Position position_;
        Offset offset_;
    };

    class Map {
    public:
        using Id = util::Tagged<std::string, Map>;
        using Roads = std::vector<Road>;
        using Buildings = std::vector<Building>;
        using Offices = std::vector<Office>;

        Map(Id id, std::string name) noexcept
            : id_(std::move(id))
            , name_(std::move(name)) {
        }

        const Id& GetId() const noexcept {
            return id_;
        }

        const std::string& GetName() const noexcept {
            return name_;
        }

        const Buildings& GetBuildings() const noexcept {
            return buildings_;
        }

        const Roads& GetRoads() const noexcept {
            return roads_;
        }

        const Offices& GetOffices() const noexcept {
            return offices_;
        }

        double GetDogSpeed() const noexcept {
            return dog_speed_;
        }

        void SetDogSpeed(double speed) noexcept {
            dog_speed_ = speed;
        }

        void AddRoad(const Road& road) {
            roads_.emplace_back(road);
        }

        void AddBuilding(const Building& building) {
            buildings_.emplace_back(building);
        }

        const boost::json::array& GetLootTypes() const noexcept {
            return loot_types_;
        }

        size_t GetLootTypesCount() const noexcept {
            return loot_types_count_;
        }

        void SetLootTypesCount(size_t count) noexcept {
            loot_types_count_ = count;
        }

        void SetLootTypes(boost::json::array loot_types) {
            loot_types_ = std::move(loot_types);
            loot_types_count_ = loot_types_.size();
        }

        size_t GetBagCapacity() const noexcept {
            return bag_capacity_;
        }

        void SetBagCapacity(size_t capacity) noexcept {
            bag_capacity_ = capacity;
        }

        const Road* FindRoadByPosition(Position position) const;
        Position GetClosestValidPosition(Position pos) const;
        bool IsOutOfBounds(Position pos) const;
        std::pair<Position, Position> GetMovementBounds() const;
        std::pair<Position, Position> GetExactMovementBounds() const;
        Position GetStartPosition() const;
        void AddOffice(Office office);
        Position GetRandomPosition() const;
        MoveResult MoveDog(Position start, Speed speed, double delta_time) const;
        bool IsAtBoundary(Position pos, Speed speed) const;

    private:
        using OfficeIdToIndex = std::unordered_map<Office::Id, size_t, util::TaggedHasher<Office::Id>>;

        Position ProjectToRoad(Position pos, const Road& road) const;

        Id id_;
        std::string name_;
        Roads roads_;
        Buildings buildings_;
        Offices offices_;
        OfficeIdToIndex warehouse_id_to_index_;
        double dog_speed_ = 0.0;
        size_t loot_types_count_ = 0;
        boost::json::array loot_types_;
        size_t bag_capacity_ = 3;
    };

    class Dog {
    public:
        using Id = util::Tagged<std::string, Dog>;

        Dog(Id id, std::string name, Map::Id map_id) noexcept
            : id_(std::move(id))
            , name_(std::move(name))
            , map_id_(std::move(map_id))
            , position_{ 0.0, 0.0 }
            , speed_{ 0.0, 0.0 }
            , direction_(Direction::NORTH) {
        }

        const Id& GetId() const noexcept {
            return id_;
        }

        const std::string& GetName() const noexcept {
            return name_;
        }

        const Map::Id& GetMapId() const noexcept {
            return map_id_;
        }

        const Position& GetPosition() const noexcept {
            return position_;
        }

        void SetPosition(Position position) noexcept {
            position_ = position;
        }

        const Speed& GetSpeed() const noexcept {
            return speed_;
        }

        void SetSpeed(Speed speed) noexcept {
            speed_ = speed;
        }

        Direction GetDirection() const noexcept {
            return direction_;
        }

        void SetDirection(Direction direction) noexcept {
            direction_ = direction;
        }

        void SetVelocity(double vx, double vy) noexcept {
            speed_ = Speed{ vx, vy };
        }

        void Stop() noexcept {
            speed_ = Speed{ 0.0, 0.0 };
        }

        bool IsMoving() const noexcept {
            return speed_.vx != 0 && speed_.vy != 0;
        }

        const Position& GetPreviousPosition() const noexcept {
            return previous_position_;
        }

        void SetPreviousPosition(Position position) noexcept {
            previous_position_ = position;
        }

    private:
        Id id_;
        std::string name_;
        Map::Id map_id_;
        Position position_;
        Speed speed_;
        Direction direction_;
        Position previous_position_{ 0.0, 0.0 };
    };

    class Player {
    public:
        using Id = util::Tagged<size_t, Player>;

        Player(Id id, Dog dog, Token token, size_t bag_capacity)
            : id_(std::move(id))
            , dog_(std::move(dog))
            , token_(std::move(token))
            , bag_capacity_(bag_capacity) {
        }

        const Id& GetId() const noexcept {
            return id_;
        }

        const Dog& GetDog() const noexcept {
            return dog_;
        }

        Dog& GetDog() noexcept {
            return dog_;
        }

        const Token& GetToken() const noexcept {
            return token_;
        }

        const std::vector<Loot>& GetBag() const noexcept {
            return bag_;
        }

        void AddToBag(const Loot& loot) {
            if (bag_.size() < bag_capacity_) {
                bag_.push_back(loot);
            }
        }

        void ClearBag() noexcept {
            bag_.clear();
        }

        bool IsBagFull() const noexcept {
            return bag_.size() >= bag_capacity_;
        }

        size_t GetBagCapacity() const noexcept {
            return bag_capacity_;
        }

        void AddScore(int delta) noexcept {
            score_ += delta;
        }

        int GetScore() const noexcept {
            return score_;
        }

        void AddPlayTime(double dt) noexcept {
            play_time_ += dt;
        }

        double GetPlayTime() const noexcept {
            return play_time_;
        }

        void AddIdleTime(double dt) noexcept {
            idle_time_ += dt;
        }

        void ResetIdleTime() noexcept {
            idle_time_ = 0.0;
        }

        double GetIdleTime() const noexcept {
            return idle_time_;
        }

    private:
        Id id_;
        Dog dog_;
        Token token_;
        std::vector<Loot> bag_;
        size_t bag_capacity_;
        int score_ = 0;

        double play_time_ = 0.0;
        double idle_time_ = 0.0;
    };

    class Game;

    class GameSession {
    public:
        using Id = util::Tagged<std::string, GameSession>;
        using MapIdHasher = util::TaggedHasher<Map::Id>;

        explicit GameSession(Id id, const Map* map, Game* game) noexcept
            : id_(std::move(id))
            , map_(map)
            , game_(game) {
        }

        const Id& GetId() const noexcept {
            return id_;
        }

        const Map* GetMap() const noexcept {
            return map_;
        }

        const std::vector<Player>& GetPlayers() const noexcept {
            return players_;
        }

        std::vector<Player>& GetPlayers() noexcept {
            return players_;
        }

        const std::vector<Loot>& GetLoots() const noexcept {
            return loots_;
        
        }

        void AddLoot(const Loot& loot) {
            loots_.push_back(loot);
        }

        void SetLootGenerator(std::unique_ptr<loot_gen::LootGenerator> generator) {
            loot_generator_ = std::move(generator);
        }

        size_t GetNextLootId() const noexcept {
            return next_loot_id_;
        }

        void UpdateState(double delta_time);

        void HandleCollisions();
        Player* FindPlayerByToken(const Token& token) noexcept;
        const Player* FindPlayerByToken(const Token& token) const noexcept;
        void AddPlayer(Player player);
        void SetNextLootId(size_t id) noexcept { next_loot_id_ = id; }
        void ClearPlayers() noexcept { players_.clear(); }
        void ClearLoots() noexcept { loots_.clear(); }


    private:
        Id id_;
        const Map* map_;
        Game* game_;
        std::vector<Player> players_;
        std::vector<Loot> loots_;
        size_t next_loot_id_ = 0;
        std::unique_ptr<loot_gen::LootGenerator> loot_generator_;
        std::unordered_map<Map::Id, boost::json::array, MapIdHasher> map_id_to_loot_types_;

        void RetireInactivePlayers();
    };

    class Game {
    public:
        using Maps = std::vector<Map>;
        using GameSessions = std::vector<GameSession>;
        using MapIdHasher = util::TaggedHasher<Map::Id>;
        using MapIdToIndex = std::unordered_map<Map::Id, size_t, MapIdHasher>;
        using RetiredPlayerCallback = std::function<void(const Player&)>;


        const Maps& GetMaps() const noexcept {
            return maps_;
        }

        const GameSessions& GetSessions() const noexcept {
            return sessions_;
        }

        void SetMapLootTypes(const Map::Id& map_id, const boost::json::array& loot_types) {
            map_id_to_loot_types_[map_id] = loot_types;
        }

        const boost::json::array* GetMapLootTypes(const Map::Id& map_id) const {
            if (auto map = FindMap(map_id)) {
                return &map->GetLootTypes();
            }
            return nullptr;
        }

        void SetDogRetirementTime(double seconds) noexcept {
            dog_retirement_time_ = seconds;
        }

        double GetDogRetirementTime() const noexcept {
            return dog_retirement_time_;
        }

        void SetRetiredPlayerCallback(RetiredPlayerCallback cb) {
            retired_player_callback_ = std::move(cb);
        }

        void OnPlayerRetired(const Player& player) const {
            if (retired_player_callback_) {
                retired_player_callback_(player);
            }
        }

        void AddMap(Map map);

        void SetLootGeneratorConfig(double base_interval, double probability);

        const Map* FindMap(const Map::Id& id) const noexcept;

        GameSession* FindSessionByMapId(const Map::Id& map_id);
        GameSession& GetOrCreateSession(const Map::Id& map_id);

        Player* FindPlayerByToken(const Token& token);
        const Player* FindPlayerByToken(const Token& token) const;

        void UpdateState(double delta_time);
        void SetTickPeriod(int64_t period);
        void StartGameLoop();
        void StopGameLoop();

    private:

        void GameLoop();

        std::vector<Map> maps_;
        MapIdToIndex map_id_to_index_;
        std::vector<GameSession> sessions_;
        std::unordered_map<Map::Id, boost::json::array, MapIdHasher> map_id_to_loot_types_;
        std::unique_ptr<loot_gen::LootGenerator> loot_generator_config_;
        std::atomic<bool> game_loop_running_{ false };
        std::thread game_loop_thread_;
        std::chrono::microseconds update_period_;
        double dog_retirement_time_ = 60.0;
        RetiredPlayerCallback retired_player_callback_;
    };

}  // namespace model