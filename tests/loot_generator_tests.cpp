#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "../src/loot_generator.h"

namespace lg = loot_gen;

TEST_CASE("LootGenerator basic functionality") {
    using namespace std::chrono;
    
    SECTION("No time passed - should generate zero loot") {
        lg::LootGenerator gen(ms(1000), 0.5);
        CHECK(gen.Generate(ms(0), 0, 10) == 0);
    }
    
    SECTION("No looters - should generate zero loot regardless of time") {
        lg::LootGenerator gen(ms(1000), 0.5);
        CHECK(gen.Generate(ms(1000), 0, 0) == 0);
        CHECK(gen.Generate(ms(5000), 5, 0) == 0);
    }
    
    SECTION("No loot shortage - should generate zero loot") {
        lg::LootGenerator gen(ms(1000), 0.5);
        // loot_count (10) >= looter_count (5) -> loot_shortage = 0
        CHECK(gen.Generate(ms(1000), 10, 5) == 0);
    }
}

TEST_CASE("LootGenerator probability calculations") {
    using namespace std::chrono;
    
    SECTION("Exact probability calculation with deterministic random") {
        lg::LootGenerator gen(ms(1000), 0.5, []() { return 0.5; });
        
        CHECK(gen.Generate(ms(1000), 0, 10) == 3);
    }
    
    SECTION("Different time intervals") {
        lg::LootGenerator gen(ms(2000), 0.8, []() { return 0.6; });
        
        auto result = gen.Generate(ms(1500), 5, 10);
        CHECK(result == 2);
    }
    
    SECTION("Maximum probability with random=1.0") {
        lg::LootGenerator gen(ms(1000), 0.5, []() { return 1.0; });
        
        CHECK(gen.Generate(ms(1000), 2, 10) == 4);
    }
    
    SECTION("Minimum probability with random=0.0") {
        lg::LootGenerator gen(ms(1000), 0.5, []() { return 0.0; });
        
        // При random=0.0, probability всегда 0
        CHECK(gen.Generate(ms(1000), 0, 10) == 0);
        CHECK(gen.Generate(ms(5000), 5, 10) == 0);
    }
}

TEST_CASE("LootGenerator time accumulation") {
    using namespace std::chrono;
    
    SECTION("Multiple short intervals accumulate") {
        lg::LootGenerator gen(ms(1000), 0.5, []() { return 1.0; });

        auto loot1 = gen.Generate(ms(500), 0, 10);  // ratio=0.5
        auto loot2 = gen.Generate(ms(500), loot1, 10); // ratio=1.0

        CHECK(loot2 >= loot1);
    }
    
    SECTION("Time resets after successful generation") {
        lg::LootGenerator gen(ms(1000), 0.5, []() { return 1.0; });
        
        // Первый вызов генерирует лут
        auto loot1 = gen.Generate(ms(1000), 0, 10);
        CHECK(loot1 > 0);
        
        auto loot2 = gen.Generate(ms(100), loot1, 10);
        CHECK(loot2 == 0);
    }
}

TEST_CASE("LootGenerator edge cases") {
    using namespace std::chrono;
    
    SECTION("Very high probability") {
        lg::LootGenerator gen(ms(1000), 0.99, []() { return 1.0; });
        
        // При очень высокой вероятности должно генерироваться почти все shortage
        auto loot = gen.Generate(ms(1000), 0, 10);
        CHECK(loot >= 8); // Должно сгенерировать большую часть shortage
    }
    
    SECTION("Very low probability") {
        lg::LootGenerator gen(ms(1000), 0.01, []() { return 1.0; });
        
        // При очень низкой вероятности генерируется мало
        auto loot = gen.Generate(ms(1000), 0, 10);
        CHECK(loot <= 2);
    }
    
    SECTION("Very long time interval") {
        lg::LootGenerator gen(ms(1000), 0.5, []() { return 1.0; });
        
        auto loot = gen.Generate(ms(10000), 0, 10);
        CHECK(loot == 10);
    }
    
    SECTION("Rounding behavior") {
        lg::LootGenerator gen(ms(1000), 0.33, []() { return 1.0; });
        
        auto loot = gen.Generate(ms(1000), 0, 3);
        CHECK(loot == 1);
    }
}

TEST_CASE("LootGenerator never exceeds looter count") {
    using namespace std::chrono;
    
    SECTION("Multiple generations don't exceed looter count") {
        lg::LootGenerator gen(ms(1000), 0.8, []() { return 1.0; });
        
        unsigned total_loot = 0;
        unsigned looter_count = 5;
        
        // Несколько вызовов подряд
        for (int i = 0; i < 10; ++i) {
            auto new_loot = gen.Generate(ms(1000), total_loot, looter_count);
            total_loot += new_loot;
            
            // В любой момент количество лута не должно превышать количество лутеров
            CHECK(total_loot <= looter_count);
        }
    }
    
    SECTION("Accumulated time doesn't cause overflow") {
        lg::LootGenerator gen(ms(1000), 0.9, []() { return 1.0; });
        
        unsigned total_loot = 0;
        unsigned looter_count = 3;
        
        // Долгое накопление времени
        auto new_loot = gen.Generate(ms(10000), total_loot, looter_count);
        total_loot += new_loot;
        
        CHECK(total_loot <= looter_count);
        CHECK(total_loot == looter_count); // Должен сгенерировать максимум
    }
}

TEST_CASE("LootGenerator with different random generators") {
    using namespace std::chrono;
    
    SECTION("Sequential random values") {
        int call_count = 0;
        auto sequential_random = [&call_count]() {
            call_count++;
            return call_count % 2 == 0 ? 0.0 : 1.0;
        };
        
        lg::LootGenerator gen(ms(1000), 0.5, sequential_random);
        
        auto loot1 = gen.Generate(ms(1000), 0, 10);
        auto loot2 = gen.Generate(ms(1000), loot1, 10);
        
        // Из-за чередования random, результаты должны быть разными
        CHECK((loot1 == 0 || loot2 == 0));
        CHECK((loot1 > 0 || loot2 > 0));
    }
    
    SECTION("Increasing random values") {
        double current = 0.1;
        auto increasing_random = [&current]() {
            current += 0.1;
            return current > 1.0 ? 1.0 : current;
        };
        
        lg::LootGenerator gen(ms(1000), 0.5, increasing_random);
        
        auto loot1 = gen.Generate(ms(1000), 0, 10);
        auto loot2 = gen.Generate(ms(1000), loot1, 10);
        
        // Второй вызов должен дать больше или равно лута из-за увеличения random
        CHECK(loot2 >= loot1);
    }
}