#define _USE_MATH_DEFINES

#include "../src/collision_detector.h"
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <vector>
#include <algorithm>
#include <cmath>

namespace collision_detector {
namespace {

class TestProvider : public ItemGathererProvider {
public:
    TestProvider(std::vector<Item> items, std::vector<Gatherer> gatherers)
        : items_(std::move(items))
        , gatherers_(std::move(gatherers)) {
    }

    size_t ItemsCount() const override {
        return items_.size();
    }

    Item GetItem(size_t idx) const override {
        return items_.at(idx);
    }

    size_t GatherersCount() const override {
        return gatherers_.size();
    }

    Gatherer GetGatherer(size_t idx) const override {
        return gatherers_.at(idx);
    }

private:
    std::vector<Item> items_;
    std::vector<Gatherer> gatherers_;
};

}  // namespace

TEST_CASE("No items") {
    TestProvider provider({}, {Gatherer{{0, 0}, {10, 0}, 1.0}});
    auto events = FindGatherEvents(provider);
    CHECK(events.empty());
}

TEST_CASE("No gatherers") {
    TestProvider provider({Item{{5, 0}, 0.5}}, {});
    auto events = FindGatherEvents(provider);
    CHECK(events.empty());
}

TEST_CASE("Single gatherer collects single item") {
    TestProvider provider({Item{{5, 0}, 0.5}}, {Gatherer{{0, 0}, {10, 0}, 1.0}});
    auto events = FindGatherEvents(provider);
    REQUIRE(events.size() == 1);
    CHECK(events[0].item_id == 0);
    CHECK(events[0].gatherer_id == 0);
    CHECK_THAT(events[0].time, Catch::Matchers::WithinAbs(0.5, 1e-9));
    CHECK_THAT(events[0].sq_distance, Catch::Matchers::WithinAbs(0.0, 1e-9));
}

TEST_CASE("Single gatherer collects multiple items") {
    TestProvider provider({
                             Item{{2, 0}, 0.5},
                             Item{{4, 0}, 0.5},
                             Item{{6, 0}, 0.5},
                         },
                         {Gatherer{{0, 0}, {10, 0}, 1.0}});
    auto events = FindGatherEvents(provider);
    REQUIRE(events.size() == 3);
    
    // Проверяем хронологический порядок и точное время
    CHECK(events[0].time <= events[1].time);
    CHECK(events[1].time <= events[2].time);
    CHECK_THAT(events[0].time, Catch::Matchers::WithinAbs(0.2, 1e-9));
    CHECK_THAT(events[1].time, Catch::Matchers::WithinAbs(0.4, 1e-9));
    CHECK_THAT(events[2].time, Catch::Matchers::WithinAbs(0.6, 1e-9));
    
    // Проверяем индексы предметов
    std::vector<size_t> item_ids;
    for (const auto& event : events) {
        item_ids.push_back(event.item_id);
    }
    std::sort(item_ids.begin(), item_ids.end());
    CHECK(item_ids == std::vector<size_t>{0, 1, 2});
}

TEST_CASE("Multiple gatherers collect items") {
    TestProvider provider({
                             Item{{5, 0}, 0.5},
                             Item{{5, 5}, 0.5},
                         },
                         {
                             Gatherer{{0, 0}, {10, 0}, 1.0},
                             Gatherer{{0, 5}, {10, 5}, 1.0},
                         });
    auto events = FindGatherEvents(provider);
    REQUIRE(events.size() == 2);
    
    // Проверяем, что каждый собиратель собрал свой предмет
    CHECK(events[0].gatherer_id != events[1].gatherer_id);
    CHECK(events[0].item_id != events[1].item_id);
    
    // Проверяем время сбора
    for (const auto& event : events) {
        CHECK_THAT(event.time, Catch::Matchers::WithinAbs(0.5, 1e-9));
    }
}

TEST_CASE("Gatherer misses item") {
    TestProvider provider({Item{{5, 2}, 0.5}}, {Gatherer{{0, 0}, {10, 0}, 1.0}});
    auto events = FindGatherEvents(provider);
    CHECK(events.empty());
}

TEST_CASE("Gatherer collects item with offset") {
    TestProvider provider({Item{{5, 1}, 0.5}}, {Gatherer{{0, 0}, {10, 0}, 1.0}});
    auto events = FindGatherEvents(provider);
    REQUIRE(events.size() == 1);
    CHECK_THAT(events[0].sq_distance, Catch::Matchers::WithinAbs(1.0, 1e-9));
    CHECK_THAT(events[0].time, Catch::Matchers::WithinAbs(0.5, 1e-9));
}

TEST_CASE("Zero movement gatherer") {
    TestProvider provider({Item{{0, 0}, 0.5}}, {Gatherer{{0, 0}, {0, 0}, 1.0}});
    auto events = FindGatherEvents(provider);
    CHECK(events.empty());
}

TEST_CASE("Events are in chronological order") {
    TestProvider provider({
                             Item{{1, 0}, 0.5},
                             Item{{3, 0}, 0.5},
                             Item{{2, 0}, 0.5},
                         },
                         {Gatherer{{0, 0}, {10, 0}, 1.0}});
    auto events = FindGatherEvents(provider);
    REQUIRE(events.size() == 3);
    
    for (size_t i = 1; i < events.size(); ++i) {
        CHECK(events[i-1].time <= events[i].time);
    }
    
    // Проверяем точное время сбора
    CHECK_THAT(events[0].time, Catch::Matchers::WithinAbs(0.1, 1e-9));
    CHECK_THAT(events[1].time, Catch::Matchers::WithinAbs(0.2, 1e-9));
    CHECK_THAT(events[2].time, Catch::Matchers::WithinAbs(0.3, 1e-9));
}

TEST_CASE("Item width affects collection") {
    SECTION("Item exactly at border should be collected") {
        TestProvider provider({Item{{5, 1}, 0.5}}, {Gatherer{{0, 0}, {10, 0}, 1.0}});
        auto events = FindGatherEvents(provider);
        CHECK(events.size() == 1);
    }
    
    SECTION("Item just inside border should be collected") {
        TestProvider provider({Item{{5, 0.9}, 0.5}}, {Gatherer{{0, 0}, {10, 0}, 1.0}});
        auto events = FindGatherEvents(provider);
        CHECK(events.size() == 1);
    }
    
    SECTION("Item just outside border should not be collected") {
        TestProvider provider({Item{{5, 1.51}, 0.5}}, {Gatherer{{0, 0}, {10, 0}, 1.0}});
        auto events = FindGatherEvents(provider);
        CHECK(events.empty());
    }
}

TEST_CASE("Item at start point") {
    TestProvider provider({Item{{0, 0}, 0.5}}, {Gatherer{{0, 0}, {10, 0}, 1.0}});
    auto events = FindGatherEvents(provider);
    REQUIRE(events.size() == 1);
    CHECK(events[0].item_id == 0);
    CHECK(events[0].gatherer_id == 0);
    CHECK_THAT(events[0].time, Catch::Matchers::WithinAbs(0.0, 1e-9));
    CHECK_THAT(events[0].sq_distance, Catch::Matchers::WithinAbs(0.0, 1e-9));
}

TEST_CASE("Item at end point") {
    TestProvider provider({Item{{10, 0}, 0.5}}, {Gatherer{{0, 0}, {10, 0}, 1.0}});
    auto events = FindGatherEvents(provider);
    REQUIRE(events.size() == 1);
    CHECK(events[0].item_id == 0);
    CHECK(events[0].gatherer_id == 0);
    CHECK_THAT(events[0].time, Catch::Matchers::WithinAbs(1.0, 1e-9));
    CHECK_THAT(events[0].sq_distance, Catch::Matchers::WithinAbs(0.0, 1e-9));
}

TEST_CASE("Two gatherers collect one item") {
    TestProvider provider(
        {Item{{5, 0.5}, 0.5}},
        {
            Gatherer{{0, 0}, {10, 0}, 0.5},
            Gatherer{{0, 1}, {10, 1}, 0.5},
        });
    auto events = FindGatherEvents(provider);
    REQUIRE(events.size() == 2);
    
    // Проверяем, что оба собирателя собрали предмет
    CHECK(events[0].item_id == 0);
    CHECK(events[1].item_id == 0);
    CHECK(events[0].gatherer_id != events[1].gatherer_id);
    
    // Проверяем время и расстояние
    for (const auto& event : events) {
        CHECK_THAT(event.time, Catch::Matchers::WithinAbs(0.5, 1e-9));
        CHECK_THAT(event.sq_distance, Catch::Matchers::WithinAbs(0.25, 1e-9));
    }
}

TEST_CASE("Item out of segment but within distance") {
    TestProvider provider({Item{{-1, 0}, 1.0}}, {Gatherer{{0, 0}, {10, 0}, 0.5}});
    auto events = FindGatherEvents(provider);
    CHECK(events.empty());
}

TEST_CASE("Item within segment but out of distance") {
    TestProvider provider({Item{{5, 1.5}, 0.5}}, {Gatherer{{0, 0}, {10, 0}, 0.5}});
    auto events = FindGatherEvents(provider);
    CHECK(events.empty());
}

TEST_CASE("Diagonal movement collects item") {
    TestProvider provider({Item{{5, 5}, 0.5}}, {Gatherer{{0, 0}, {10, 10}, 1.0}});
    auto events = FindGatherEvents(provider);
    REQUIRE(events.size() == 1);
    CHECK(events[0].item_id == 0);
    CHECK(events[0].gatherer_id == 0);
    CHECK_THAT(events[0].time, Catch::Matchers::WithinAbs(0.5, 1e-9));
    CHECK_THAT(events[0].sq_distance, Catch::Matchers::WithinAbs(0.0, 1e-9));
}

TEST_CASE("Diagonal movement misses item") {
    TestProvider provider({Item{{5, 7.0}, 0.5}}, {Gatherer{{0, 0}, {10, 10}, 0.5}});
    auto events = FindGatherEvents(provider);
    CHECK(events.empty());
}

TEST_CASE("Multiple items with different widths") {
    TestProvider provider({
        Item{{2, 0}, 0.3},
        Item{{4, 1}, 0.3},
        Item{{6, 0}, 0.7},
    }, {Gatherer{{0, 0}, {10, 0}, 0.5}});
    
    auto events = FindGatherEvents(provider);
    REQUIRE(events.size() == 2);
    
    // Проверяем хронологический порядок
    CHECK(events[0].time <= events[1].time);
    
    // Проверяем, что собраны правильные предметы
    std::vector<size_t> collected_items;
    for (const auto& event : events) {
        collected_items.push_back(event.item_id);
    }
    std::sort(collected_items.begin(), collected_items.end());
    CHECK(collected_items == std::vector<size_t>{0, 2});
    
    // Проверяем время сбора
    CHECK_THAT(events[0].time, Catch::Matchers::WithinAbs(0.2, 1e-9));
    CHECK_THAT(events[1].time, Catch::Matchers::WithinAbs(0.6, 1e-9));
}

TEST_CASE("Gatherer with zero width") {
    TestProvider provider({
        Item{{5, 0}, 0.5},
        Item{{5, 0.6}, 0.5},
    }, {Gatherer{{0, 0}, {10, 0}, 0.0}});
    
    auto events = FindGatherEvents(provider);
    REQUIRE(events.size() == 1);
    CHECK(events[0].item_id == 0);
}

TEST_CASE("Item with zero width") {
    TestProvider provider({
        Item{{5, 0.4}, 0.0},
        Item{{5, 0.6}, 0.0},
    }, {Gatherer{{0, 0}, {10, 0}, 0.5}});
    
    auto events = FindGatherEvents(provider);
    REQUIRE(events.size() == 1);
    CHECK(events[0].item_id == 0);
}

TEST_CASE("Same time events ordering") {
    TestProvider provider({
        Item{{1, 1}, 0.5},
        Item{{1, -1}, 0.5},
        Item{{1, 0}, 0.5},
    }, {Gatherer{{0, 0}, {2, 0}, 1.0}});
    
    auto events = FindGatherEvents(provider);
    REQUIRE(events.size() == 3);
    
    // Все события должны иметь одинаковое время
    for (size_t i = 1; i < events.size(); ++i) {
        CHECK_THAT(events[i].time, Catch::Matchers::WithinAbs(events[0].time, 1e-9));
    }
    
    // Проверяем, что все предметы собраны
    std::vector<size_t> item_ids;
    for (const auto& event : events) {
        item_ids.push_back(event.item_id);
    }
    std::sort(item_ids.begin(), item_ids.end());
    CHECK(item_ids == std::vector<size_t>{0, 1, 2});
}

TEST_CASE("Complex scenario with multiple gatherers and items") {
    TestProvider provider({
        Item{{2, 0}, 0.5},
        Item{{4, 1}, 0.6},
        Item{{6, -1}, 0.4},
        Item{{8, 0}, 0.3},
    }, {
        Gatherer{{0, 0}, {10, 0}, 0.5},
        Gatherer{{0, 1}, {10, 1}, 0.5},
        Gatherer{{0, -1}, {10, -1}, 0.5},
    });
    
    auto events = FindGatherEvents(provider);
    
    // Проверяем хронологический порядок
    for (size_t i = 1; i < events.size(); ++i) {
        CHECK(events[i-1].time <= events[i].time);
    }
    
    // Проверяем корректность индексов и данных
    for (const auto& event : events) {
        CHECK(event.item_id < 4);
        CHECK(event.gatherer_id < 3);
        CHECK(event.time >= 0.0);
        CHECK(event.time <= 1.0);
        CHECK(event.sq_distance >= 0.0);
    }
}

// Критические тесты для отсеивания неправильных реализаций
TEST_CASE("Exact projection ratio calculation") {
    // Предмет должен быть собран в точно вычисленное время
    TestProvider provider({Item{{3, 0}, 0.5}}, {Gatherer{{0, 0}, {10, 0}, 1.0}});
    auto events = FindGatherEvents(provider);
    REQUIRE(events.size() == 1);
    CHECK_THAT(events[0].time, Catch::Matchers::WithinAbs(0.3, 1e-9));
}

TEST_CASE("Item exactly at border with zero width gatherer") {
    TestProvider provider({Item{{5, 0}, 0.0}}, {Gatherer{{0, 0}, {10, 0}, 0.0}});
    auto events = FindGatherEvents(provider);
    REQUIRE(events.size() == 1);
    CHECK_THAT(events[0].time, Catch::Matchers::WithinAbs(0.5, 1e-9));
}

TEST_CASE("Item very close but outside radius") {
    TestProvider provider({Item{{5, 1.5001}, 0.5}}, {Gatherer{{0, 0}, {10, 0}, 1.0}});
    auto events = FindGatherEvents(provider);
    CHECK(events.empty());
}

TEST_CASE("Multiple items same position different widths") {
    TestProvider provider({
        Item{{5, 0}, 0.4},
        Item{{5, 0}, 0.6},
    }, {Gatherer{{0, 0}, {10, 0}, 0.5}});
    
    auto events = FindGatherEvents(provider);
    // Оба предмета должны быть собраны, так как они находятся на линии движения
    // и расстояние равно 0, что меньше суммы ширины собирателя и ширины предмета
    REQUIRE(events.size() == 2);
    
    // Проверяем, что собраны оба предмета
    std::vector<size_t> item_ids;
    for (const auto& event : events) {
        item_ids.push_back(event.item_id);
    }
    std::sort(item_ids.begin(), item_ids.end());
    CHECK(item_ids == std::vector<size_t>{0, 1});
}

TEST_CASE("Gatherer width exactly matches distance") {
    TestProvider provider({Item{{5, 1.0}, 0.5}}, {Gatherer{{0, 0}, {10, 0}, 0.5}});
    auto events = FindGatherEvents(provider);
    // 1.0 <= (0.5 + 0.5)^2 = 1.0 -> должен быть собран
    CHECK(events.size() == 1);
}

TEST_CASE("Item not collected when widths sum less than distance") {
    TestProvider provider({Item{{5, 1.1}, 0.5}}, {Gatherer{{0, 0}, {10, 0}, 0.5}});
    auto events = FindGatherEvents(provider);
    // 1.1^2 = 1.21 > (0.5 + 0.5)^2 = 1.0 -> не должен быть собран
    CHECK(events.empty());
}

}  // namespace collision_detector