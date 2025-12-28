#include "model.h"
#include <stdexcept>
#include <algorithm>
#include <random>
#include <cmath>
#include <unordered_set>
#include <functional>
#include <iostream>
#include <numeric>
#include <string>
#include <vector>

namespace model {
    using namespace std::literals;
    using namespace geom;



    bool Road::IsPositionInRoad(Position pos) const {
        auto borders = GetBorders();
        bool more_than_min = ((pos.x >= borders.first.x) && (pos.y >= borders.first.y));
        bool less_than_max = ((pos.x <= borders.second.x) && (pos.y <= borders.second.y));
        return more_than_min && less_than_max;
    }

    std::pair<Position, Position> Road::GetBorders() const {
        Position min, max;

        min.x = static_cast<Coord>(GetMinX() - width_);
        min.y = static_cast<Coord>(GetMinY() - width_);
        max.x = static_cast<Coord>(GetMaxX() + width_);
        max.y = static_cast<Coord>(GetMaxY() + width_);

        return { min, max };
    }

    const Road* Map::FindRoadByPosition(Position position) const {
        for (const auto& road : roads_) {
            if (road.IsPositionInRoad(position)) {
                return &road;
            }
        }
        return nullptr;
    }

    bool Map::IsOutOfBounds(Position pos) const {
        auto [min_bound, max_bound] = GetMovementBounds();
        return pos.x < min_bound.x || pos.x > max_bound.x ||
            pos.y < min_bound.y || pos.y > max_bound.y;
    }

    Position Map::GetClosestValidPosition(Position pos) const {
        if (roads_.empty()) {
            return pos;
        }

        // Получаем границы движения
        auto [min_bound, max_bound] = GetMovementBounds();

        // Ограничиваем позицию границами
        pos.x = std::max(min_bound.x, std::min(pos.x, max_bound.x));
        pos.y = std::max(min_bound.y, std::min(pos.y, max_bound.y));

        // Ищем ближайшую дорогу
        const Road* closest_road = nullptr;
        double min_distance = std::numeric_limits<double>::max();

        for (const auto& road : roads_) {
            if (road.IsPositionInRoad(pos)) {
                return pos; // Позиция уже на дороге
            }

            // Вычисляем расстояние до дороги
            double distance = CalculateDistanceToRoad(pos, road);
            if (distance < min_distance) {
                min_distance = distance;
                closest_road = &road;
            }
        }

        if (closest_road) {
            // Проецируем на ближайшую дорогу
            return ProjectToRoad(pos, *closest_road);
        }

        return pos;
    }

    void Map::AddOffice(Office office) {
        if (warehouse_id_to_index_.contains(office.GetId())) {
            throw std::invalid_argument("Duplicate warehouse");
        }

        const size_t index = offices_.size();
        Office& o = offices_.emplace_back(std::move(office));
        try {
            warehouse_id_to_index_.emplace(o.GetId(), index);
        }
        catch (...) {
            offices_.pop_back();
            throw;
        }
    }

    std::pair<Position, Position> Map::GetExactMovementBounds() const {
        if (roads_.empty()) {
            return { Position{0.0, 0.0}, Position{0.0, 0.0} };
        }

        double min_x = std::numeric_limits<double>::max();
        double max_x = std::numeric_limits<double>::lowest();
        double min_y = std::numeric_limits<double>::max();
        double max_y = std::numeric_limits<double>::lowest();

        for (const auto& road : roads_) {
            min_x = std::min(min_x, road.GetMinX() - road.GetWidth());
            max_x = std::max(max_x, road.GetMaxX() + road.GetWidth());
            min_y = std::min(min_y, road.GetMinY() - road.GetWidth());
            max_y = std::max(max_y, road.GetMaxY() + road.GetWidth());
        }

        return {
            Position{min_x, min_y},
            Position{max_x, max_y}
        };
    }

    Position Map::GetStartPosition() const {
        if (roads_.empty()) {
            return Position{ 0.0, 0.0 };
        }

        const auto& first_road = roads_[0];
        auto start_point = first_road.GetStart();

        return Position{
            static_cast<double>(start_point.x),
            static_cast<double>(start_point.y)
        };
    }



    Position Map::GetRandomPosition() const {
        if (roads_.empty()) {
            return Position{ 0.0, 0.0 };
        }

        static std::random_device rd;
        static std::mt19937 gen(rd());

        std::uniform_int_distribution<size_t> road_dist(0, roads_.size() - 1);
        const auto& road = roads_[road_dist(gen)];

        auto start = road.GetStart();
        auto end = road.GetEnd();


        const double tolerance = 0.4;

        if (road.IsHorizontal()) {
            double min_x = std::min(static_cast<double>(start.x), static_cast<double>(end.x)) + tolerance;
            double max_x = std::max(static_cast<double>(start.x), static_cast<double>(end.x)) - tolerance;

            if (min_x >= max_x) {
                min_x = static_cast<double>(start.x);
                max_x = static_cast<double>(end.x);
            }

            std::uniform_real_distribution<double> pos_dist(min_x, max_x);
            return Position{ pos_dist(gen), static_cast<double>(start.y) };
        }
        else {
            double min_y = std::min(static_cast<double>(start.y), static_cast<double>(end.y)) + tolerance;
            double max_y = std::max(static_cast<double>(start.y), static_cast<double>(end.y)) - tolerance;

            if (min_y >= max_y) {
                min_y = static_cast<double>(start.y);
                max_y = static_cast<double>(end.y);
            }

            std::uniform_real_distribution<double> pos_dist(min_y, max_y);
            return Position{ static_cast<double>(start.x), pos_dist(gen) };
        }
    }

    bool Map::IsAtBoundary(Position pos, Speed speed) const {
        auto [min_bound, max_bound] = GetExactMovementBounds();
        const double tolerance = 1e-5;

        // Проверяем достижение границы с учетом направления движения
        if (speed.vx > 0 && std::abs(pos.x - max_bound.x) < tolerance) {
            return true; // Достигли правой границы
        }
        if (speed.vx < 0 && std::abs(pos.x - min_bound.x) < tolerance) {
            return true; // Достигли левой границы
        }
        if (speed.vy > 0 && std::abs(pos.y - max_bound.y) < tolerance) {
            return true; // Достигли нижней границы
        }
        if (speed.vy < 0 && std::abs(pos.y - min_bound.y) < tolerance) {
            return true; // Достигли верхней границы
        }
        return false;
    }

    std::pair<Position, Position> Map::GetMovementBounds() const {
        if (roads_.empty()) {
            return { Position{0.0, 0.0}, Position{0.0, 0.0} };
        }

        double min_x = std::numeric_limits<double>::max();
        double max_x = std::numeric_limits<double>::lowest();
        double min_y = std::numeric_limits<double>::max();
        double max_y = std::numeric_limits<double>::lowest();

        for (const auto& road : roads_) {
            min_x = std::min(min_x, road.GetMinX() - road.GetWidth());
            max_x = std::max(max_x, road.GetMaxX() + road.GetWidth());
            min_y = std::min(min_y, road.GetMinY() - road.GetWidth());
            max_y = std::max(max_y, road.GetMaxY() + road.GetWidth());
        }

        return {
            Position{min_x, min_y},
            Position{max_x, max_y}
        };
    }

    MoveResult Map::MoveDog(Position start, Speed speed, double delta_time) const {

        MoveResult result;
        result.hit_boundary = false;

        // Проверка наличия дорог
        if (roads_.empty()) {
            result.position = start;
            return result;
        }

        // Рассчитываем новую позицию
        Position target{
            start.x + speed.vx * delta_time,
            start.y + speed.vy * delta_time
        };

        // Получаем границы движения карты
        auto [min_bound, max_bound] = GetExactMovementBounds();

        // Ограничиваем позицию границами карты
        Position final_position = target;
        {
            // Проверяем выход за границы карты по X
            if (target.x < min_bound.x) {
                final_position.x = min_bound.x;
                result.hit_boundary = true;
            }
            else if (target.x > max_bound.x) {
                final_position.x = max_bound.x;
                result.hit_boundary = true;
            }

            // Проверяем выход за границы карты по Y
            if (target.y < min_bound.y) {
                final_position.y = min_bound.y;
                result.hit_boundary = true;
            }
            else if (target.y > max_bound.y) {
                final_position.y = max_bound.y;
                result.hit_boundary = true;
            }
        }

        // Проверяем, находится ли конечная позиция на какой-либо дороге
        bool is_on_road = false;
        // Несколько дорог(собака может находиться на углу, угол это 2 дороги)
        Roads cur_roads;

        for (const auto& road : roads_) {
            if (road.IsPositionInRoad(final_position)) {
                // Если конечная позиция на дороге, разрешаем движение
                result.position = final_position;
                //result.hit_boundary = false;
                return result;
            }
            if (road.IsPositionInRoad(start)) {
                cur_roads.push_back(road);
            }
        }

        // Если конечная позиция не на дороге, ищем ближайшую валидную позицию на дорогах
        Position best_position = start; // По умолчанию остаёмся на месте
        double min_distance_sq = std::numeric_limits<double>::max();

        // Проверка конечной позиции по доронам
        for (const auto& road : cur_roads) {
            // Для горизонтальной дороги и движения по вертикали
            if (road.IsHorizontal() && speed.vy != 0) {
                double road_y;
                if (speed.vy > 0) {
                    road_y = static_cast<double>(road.GetStart().y + road.GetWidth());
                }
                else {
                    road_y = static_cast<double>(road.GetStart().y - road.GetWidth());
                }
                double road_min_x = road.GetMinX() - road.GetWidth();
                double road_max_x = road.GetMaxX() + road.GetWidth();

                // Проецируем позицию на дорогу
                Position projected{ std::clamp(final_position.x, road_min_x, road_max_x), road_y };

                // Проверяем, что проекция находится на дороге
                if (road.IsPositionInRoad(projected)) {
                    double distance_sq = (final_position.x - projected.x) * (final_position.x - projected.x) +
                        (final_position.y - projected.y) * (final_position.y - projected.y);

                    if (distance_sq < min_distance_sq) {
                        min_distance_sq = distance_sq;
                        best_position = projected;
                    }
                }
            }
            // Для горизонтальной дороги и движения по горизонтале
            else if (road.IsHorizontal() && speed.vx != 0) {
                double road_y = static_cast<double>(road.GetStart().y) + road.GetWidth();


                double road_min_x = road.GetMinX() - road.GetWidth();
                double road_max_x = road.GetMaxX() + road.GetWidth();


                // Проецируем позицию на дорогу
                Position projected{ std::clamp(final_position.x, road_min_x, road_max_x), road_y };

                //Проверяем, что проекция находится на дороге
                if (road.IsPositionInRoad(projected)) {
                    double distance_sq = (final_position.x - projected.x) * (final_position.x - projected.x) +
                        (final_position.y - projected.y) * (final_position.y - projected.y);

                    if (distance_sq < min_distance_sq) {
                        min_distance_sq = distance_sq;
                        best_position = projected;
                    }
                }
            }
            // Для вертикальной дороги и движения по горизонтале
            else if (road.IsVertical() && speed.vx != 0) {
                double road_x;
                if (speed.vx > 0) {
                    road_x = static_cast<double>(road.GetStart().x + road.GetWidth());
                }
                else {
                    road_x = static_cast<double>(road.GetStart().x - road.GetWidth());
                }
                double road_min_y = road.GetMinY() - road.GetWidth();
                double road_max_y = road.GetMaxY() + road.GetWidth();

                // Проецируем позицию на дорогу
                Position projected{ road_x, std::clamp(final_position.y, road_min_y, road_max_y) };

                // Проверяем, что проекция находится на дороге
                if (road.IsPositionInRoad(projected)) {
                    double distance_sq = (final_position.x - projected.x) * (final_position.x - projected.x) +
                        (final_position.y - projected.y) * (final_position.y - projected.y);

                    if (distance_sq < min_distance_sq) {
                        min_distance_sq = distance_sq;
                        best_position = projected;
                    }
                }
            }
            // Для вертикальной дороги и движения по вертикали
            else if (road.IsVertical() && speed.vy != 0) {
                double road_x = static_cast<double>(road.GetStart().x) + road.GetWidth();


                double road_min_y = road.GetMinY() - road.GetWidth();
                double road_max_y = road.GetMaxY() + road.GetWidth();

                // Проецируем позицию на дорогу
                Position projected{ final_position.x , std::clamp(final_position.y, road_min_y, road_max_y) };

                // Проверяем, что проекция находится на дороге
                if (road.IsPositionInRoad(projected)) {
                    double distance_sq = (final_position.x - projected.x) * (final_position.x - projected.x) +
                        (final_position.y - projected.y) * (final_position.y - projected.y);

                    if (distance_sq < min_distance_sq) {
                        min_distance_sq = distance_sq;
                        best_position = projected;
                    }
                }
            }
        }

        // Проверяем, достигли ли мы границы дороги
        bool hit_road_boundary = (best_position.x != final_position.x) || (best_position.y != final_position.y);

        result.position = best_position;
        result.hit_boundary = result.hit_boundary || hit_road_boundary;

        return result;

    }

    double CalculateDistanceToRoad(Position pos, const Road& road) {
        if (road.IsHorizontal()) {
            double road_y = static_cast<double>(road.GetStart().y);
            double road_min_x = static_cast<double>(std::min(road.GetStart().x, road.GetEnd().x));
            double road_max_x = static_cast<double>(std::max(road.GetStart().x, road.GetEnd().x));

            // Расстояние по Y - просто разница координат Y
            double y_dist = std::abs(pos.y - road_y);
            double x_dist = 0.0;
            if (pos.x < road_min_x) {
                x_dist = road_min_x - pos.x;
            }
            else if (pos.x > road_max_x) {
                x_dist = pos.x - road_max_x;
            }

            return std::sqrt(y_dist * y_dist + x_dist * x_dist);
        }
        else {
            double road_x = static_cast<double>(road.GetStart().x);
            double road_min_y = static_cast<double>(std::min(road.GetStart().y, road.GetEnd().y));
            double road_max_y = static_cast<double>(std::max(road.GetStart().y, road.GetEnd().y));

            // Расстояние по X - просто разница координат X
            double x_dist = std::abs(pos.x - road_x);
            double y_dist = 0.0;
            if (pos.y < road_min_y) {
                y_dist = road_min_y - pos.y;
            }
            else if (pos.y > road_max_y) {
                y_dist = pos.y - road_max_y;
            }

            return std::sqrt(x_dist * x_dist + y_dist * y_dist);
        }
    }

    Position Map::ProjectToRoad(Position pos, const Road& road) const {
        if (road.IsHorizontal()) {
            double road_y = static_cast<double>(road.GetStart().y);
            double road_min_x = static_cast<double>(std::min(road.GetStart().x, road.GetEnd().x));
            double road_max_x = static_cast<double>(std::max(road.GetStart().x, road.GetEnd().x));

            double projected_x = std::max(road_min_x, std::min(pos.x, road_max_x));
            return Position{ projected_x, road_y };
        }
        else {
            double road_x = static_cast<double>(road.GetStart().x);
            double road_min_y = static_cast<double>(std::min(road.GetStart().y, road.GetEnd().y));
            double road_max_y = static_cast<double>(std::max(road.GetStart().y, road.GetEnd().y));

            double projected_y = std::max(road_min_y, std::min(pos.y, road_max_y));
            return Position{ road_x, projected_y };
        }
    }

    Player* GameSession::FindPlayerByToken(const Token& token) noexcept {
        auto it = std::find_if(players_.begin(), players_.end(),
            [&token](const Player& player) {
                return player.GetToken() == token;
            });
        return it != players_.end() ? &*it : nullptr;
    }

    const Player* GameSession::FindPlayerByToken(const Token& token) const noexcept {
        auto it = std::find_if(players_.begin(), players_.end(),
            [&token](const Player& player) {
                return player.GetToken() == token;
            });
        return it != players_.end() ? &*it : nullptr;
    }

    void GameSession::AddPlayer(Player player) {
        players_.push_back(std::move(player));
    }

    void GameSession::UpdateState(double delta_time) {
        // Обновляем игровое время и время бездействия
        for (auto& player : players_) {
            // Общее время в игре
            player.AddPlayTime(delta_time);

            // Проверяем скорость собаки
            auto& dog = player.GetDog();
            auto speed = dog.GetSpeed();

            constexpr double EPS = 1e-10;
            bool is_idle = std::abs(speed.vx) < EPS && std::abs(speed.vy) < EPS;

            if (is_idle) {
                player.AddIdleTime(delta_time);
            }
            else {
                player.ResetIdleTime();
            }
        }

        // Генерация нового лута
        if (loot_generator_) {
            auto time_delta = std::chrono::duration<double>(delta_time);
            auto new_loot_count = loot_generator_->Generate(
                std::chrono::duration_cast<loot_gen::LootGenerator::TimeInterval>(time_delta),
                loots_.size(),
                players_.size()
            );

            // Создаем новые предметы лута
            for (unsigned i = 0; i < new_loot_count; ++i) {
                // Генерируем случайный тип лута
                std::uniform_int_distribution<size_t> dist(0, map_->GetLootTypesCount() - 1);
                size_t type = dist(random_engine);

                // Получаем случайную позицию на карте
                Position pos = map_->GetRandomPosition();

                // Получаем стоимость лута из конфигурации карты
                int value = 0;
                auto loot_types = map_->GetLootTypes();
                if (type < loot_types.size()) {
                    auto& loot_type = loot_types[type].as_object();
                    if (loot_type.contains("value")) {
                        value = static_cast<int>(loot_type.at("value").as_int64());
                    }
                }

                // Создаем лут с уникальным ID и стоимостью
                Loot loot(Loot::Id{ next_loot_id_++ }, type, pos, value);
                loots_.push_back(std::move(loot));
            }
        }

        // Сохраняем предыдущие позиции игроков
        for (auto& player : players_) {
            auto& dog = player.GetDog();
            dog.SetPreviousPosition(dog.GetPosition());
        }

        // Обновляем позиции игроков
        for (auto& player : players_) {
            auto& dog = player.GetDog();
            auto current_position = dog.GetPosition();
            auto speed = dog.GetSpeed();

            if (dog.IsMoving() || std::abs(speed.vx) > 1e-10 || std::abs(speed.vy) > 1e-10) {
                auto move_result = map_->MoveDog(current_position, speed, delta_time);
                dog.SetPosition(move_result.position);

                if (move_result.hit_boundary) {
                    dog.Stop();
                }
            }
        }

        // Обрабатываем сбор предметов и возвращение на базу
        HandleCollisions();

        // Проверяем, кто «ушёл на покой»
        RetireInactivePlayers();
    }

    void GameSession::RetireInactivePlayers() {
        if (!game_) {
            return;
        }

        const double retire_time = game_->GetDogRetirementTime();

        std::vector<Player> active_players;
        active_players.reserve(players_.size());

        for (auto& player : players_) {
            if (player.GetIdleTime() >= retire_time) {
                // игрок уходит на покой: уведомляем Game
                if (game_) {
                    game_->OnPlayerRetired(player);
                }
                // НЕ переносим его в active_players
            }
            else {
                active_players.push_back(std::move(player));
            }
        }

        players_.swap(active_players);
    }


    void GameSession::HandleCollisions() {
        // Структура для хранения событий игры
        struct GameEvent {
            double time; // Время события (0-1, где 0 - начало тика, 1 - конец)
            enum Type { LOOT, OFFICE } type; // Тип события: сбор предмета или возвращение на базу
            size_t gatherer_id; // ID игрока (собирателя)
            size_t item_id; // ID предмета или офиса

            // Для сортировки событий по времени
            bool operator<(const GameEvent& other) const {
                return time < other.time;
            }
        };

        // Провайдер для обнаружения сбора предметов
        class LootProvider : public collision_detector::ItemGathererProvider {
        public:
            LootProvider(const std::vector<Loot>& loots, const std::vector<Player>& players)
                : loots_(loots), players_(players) {
            }

            size_t ItemsCount() const override {
                return loots_.size();
            }

            collision_detector::Item GetItem(size_t idx) const override {
                // Предметы имеют нулевую ширину (по условию)
                return { loots_[idx].position, 0.0 };
            }

            size_t GatherersCount() const override {
                return players_.size();
            }

            collision_detector::Gatherer GetGatherer(size_t idx) const override {
                const auto& dog = players_[idx].GetDog();
                // Игроки имеют ширину 0.6 (по условию)
                return { dog.GetPreviousPosition(), dog.GetPosition(), 0.6 };
            }

        private:
            const std::vector<Loot>& loots_;
            const std::vector<Player>& players_;
        };

        // Провайдер для обнаружения возвращения на базу (офисы)
        class OfficeProvider : public collision_detector::ItemGathererProvider {
        public:
            OfficeProvider(const std::vector<Office>& offices, const std::vector<Player>& players)
                : offices_(offices), players_(players) {
            }

            size_t ItemsCount() const override {
                return offices_.size();
            }

            collision_detector::Item GetItem(size_t idx) const override {
                // Офисы имеют ширину 0.5 (по условию)
                return { offices_[idx].GetPosition(), 0.5 };
            }

            size_t GatherersCount() const override {
                return players_.size();
            }

            collision_detector::Gatherer GetGatherer(size_t idx) const override {
                const auto& dog = players_[idx].GetDog();
                // Игроки имеют ширину 0.6 (по условию)
                return { dog.GetPreviousPosition(), dog.GetPosition(), 0.6 };
            }

        private:
            const std::vector<Office>& offices_;
            const std::vector<Player>& players_;
        };

        // Находим события сбора предметов
        LootProvider loot_provider(loots_, players_);
        auto loot_events = collision_detector::FindGatherEvents(loot_provider);

        // Находим события возвращения на базу
        OfficeProvider office_provider(map_->GetOffices(), players_);
        auto office_events = collision_detector::FindGatherEvents(office_provider);

        // Собираем все события в один список
        std::vector<GameEvent> all_events;

        // Добавляем события сбора предметов
        for (const auto& event : loot_events) {
            all_events.push_back({ event.time, GameEvent::LOOT, event.gatherer_id, event.item_id });
        }

        // Добавляем события возвращения на базу
        for (const auto& event : office_events) {
            all_events.push_back({ event.time, GameEvent::OFFICE, event.gatherer_id, event.item_id });
        }

        // Сортируем события по времени (хронологический порядок)
        std::sort(all_events.begin(), all_events.end());

        // Множество для отслеживания уже собранных предметов
        std::unordered_set<Loot::Id, util::TaggedHasher<Loot::Id>> collected_loots;

        // Обрабатываем события в хронологическом порядке
        for (const auto& event : all_events) {
            auto& player = players_[event.gatherer_id];

            if (event.type == GameEvent::LOOT) {
                // Проверяем корректность индексов
                if (event.gatherer_id >= players_.size() || event.item_id >= loots_.size()) {
                    continue;
                }
                
                auto& player = players_[event.gatherer_id];
                const auto& loot = loots_[event.item_id];
                
                // Проверяем, что лут еще не был собран
                if (collected_loots.find(loot.id) != collected_loots.end()) {
                    continue;
                }
                
                // Проверяем, что у игрока есть место в рюкзаке
                if (!player.IsBagFull()) {
                    // Добавляем лут в рюкзак игрока
                    player.AddToBag(loot);
                    
                    // Помечаем лут как собранный
                    collected_loots.insert(loot.id);
                    
                    // Таймер бездействия будет сброшен в UpdateState, если собака двигается
                    // или продолжит увеличиваться, если собака стоит
                }
            }
            else {
                // Суммируем стоимость всех предметов в рюкзаке
                int total_score = std::accumulate(
                    player.GetBag().begin(), player.GetBag().end(), 0,
                    [](int sum, const Loot& loot) {
                        return sum + loot.value;
                    });


                // Начисляем очки игроку
                player.AddScore(total_score);

                // Очищаем рюкзак
                player.ClearBag();
            }
        }

        // Удаляем собранные предметы из мира
        loots_.erase(
            std::remove_if(loots_.begin(), loots_.end(),
                [&collected_loots](const Loot& loot) {
                    return collected_loots.find(loot.id) != collected_loots.end();
                }),
            loots_.end());
    }
    

    void Game::AddMap(Map map) {
        const size_t index = maps_.size();
        if (auto [it, inserted] = map_id_to_index_.emplace(map.GetId(), index); !inserted) {
            throw std::invalid_argument("Map with id "s + *map.GetId() + " already exists"s);
        }
        else {
            try {
                maps_.emplace_back(std::move(map));
            }
            catch (...) {
                map_id_to_index_.erase(it);
                throw;
            }
        }
    }

    const Map* Game::FindMap(const Map::Id& id) const noexcept {
        if (auto it = map_id_to_index_.find(id); it != map_id_to_index_.end()) {
            return &maps_.at(it->second);
        }
        return nullptr;
    }

    void Game::SetLootGeneratorConfig(double base_interval, double probability) {
        loot_generator_config_ = std::make_unique<loot_gen::LootGenerator>(
            std::chrono::duration_cast<loot_gen::LootGenerator::TimeInterval>(
                std::chrono::duration<double>(base_interval)),
            probability,
            []() {
                static std::random_device rd;
                static std::mt19937 gen(rd());
                static std::uniform_real_distribution<double> dist(0.0, 1.0);
                return dist(gen);
            }
        );
    }

    void Game::GameLoop() {
        using namespace std::chrono;
        auto last_tick_time = steady_clock::now();

        while (game_loop_running_) {
            auto current_time = steady_clock::now();
            auto delta_time = duration_cast<duration<double>>(current_time - last_tick_time).count();
            last_tick_time = current_time;

            // Обновляем состояние игры
            UpdateState(delta_time);

            std::this_thread::sleep_for(update_period_);
        }
    }

    GameSession* Game::FindSessionByMapId(const Map::Id& map_id) {
        auto it = std::find_if(sessions_.begin(), sessions_.end(),
            [&map_id](const GameSession& session) {
                return session.GetMap()->GetId() == map_id;
            });
        return it != sessions_.end() ? &*it : nullptr;
    }

    GameSession& Game::GetOrCreateSession(const Map::Id& map_id) {
        if (auto session = FindSessionByMapId(map_id)) {
            return *session;
        }

        if (auto map = FindMap(map_id)) {
            sessions_.emplace_back(GameSession::Id{ *map_id + "_session" }, map, this);
            auto& session = sessions_.back();

            if (loot_generator_config_) {
                session.SetLootGenerator(std::make_unique<loot_gen::LootGenerator>(*loot_generator_config_));
            }

            return session;
        }
        throw std::invalid_argument("Map not found");
    }

    Player* Game::FindPlayerByToken(const Token& token) {
        for (auto& session : sessions_) {
            if (auto player = session.FindPlayerByToken(token)) {
                return player;
            }
        }
        return nullptr;
    }

    const Player* Game::FindPlayerByToken(const Token& token) const {
        for (const auto& session : sessions_) {
            if (auto player = session.FindPlayerByToken(token)) {
                return player;
            }
        }
        return nullptr;
    }

    void Game::UpdateState(double delta_time) {
        for (auto& session : sessions_) {
            session.UpdateState(delta_time);
        }
    }

    void Game::SetTickPeriod(int64_t period) {
        update_period_ = std::chrono::microseconds(period);
    }

    void Game::StartGameLoop() {
        if (game_loop_running_) return;

        game_loop_running_ = true;
        game_loop_thread_ = std::thread([this]() { GameLoop(); });
    }

    void Game::StopGameLoop() {
        game_loop_running_ = false;
        if (game_loop_thread_.joinable()) {
            game_loop_thread_.join();
        }
    }

}  // namespace model