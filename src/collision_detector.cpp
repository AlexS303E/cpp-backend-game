#include "collision_detector.h"
#include <cassert>

namespace collision_detector {

    CollectionResult TryCollectPoint(Point2D a, Point2D b, Point2D c) {
        // Проверим, что перемещение ненулевое.
        // Тут приходится использовать строгое равенство, а не приближённое,
        // пскольку при сборе заказов придётся учитывать перемещение даже на небольшое
        // расстояние.
        assert(b.x != a.x || b.y != a.y);
        const double u_x = c.x - a.x;
        const double u_y = c.y - a.y;
        const double v_x = b.x - a.x;
        const double v_y = b.y - a.y;
        const double u_dot_v = u_x * v_x + u_y * v_y;
        const double u_len2 = u_x * u_x + u_y * u_y;
        const double v_len2 = v_x * v_x + v_y * v_y;
        const double proj_ratio = u_dot_v / v_len2;
        const double sq_distance = u_len2 - (u_dot_v * u_dot_v) / v_len2;

        return CollectionResult(sq_distance, proj_ratio);
    }

    std::vector<GatheringEvent> FindGatherEvents(const ItemGathererProvider& provider) {
        std::vector<GatheringEvent> events;

        for (size_t gatherer_idx = 0; gatherer_idx < provider.GatherersCount(); ++gatherer_idx) {
            auto gatherer = provider.GetGatherer(gatherer_idx);

            // Пропускаем собирателей с нулевым перемещением
            if (gatherer.start_pos.x == gatherer.end_pos.x && gatherer.start_pos.y == gatherer.end_pos.y) {
                continue;
            }

            for (size_t item_idx = 0; item_idx < provider.ItemsCount(); ++item_idx) {
                auto item = provider.GetItem(item_idx);

                auto result = TryCollectPoint(gatherer.start_pos, gatherer.end_pos, item.position);

                // Проверяем условия сбора:
                // 1. proj_ratio между 0 и 1 (точка проекции находится на отрезке движения)
                // 2. Квадрат расстояния меньше или равен квадрату радиуса собирателя
                if (result.proj_ratio >= 0 && result.proj_ratio <= 1 &&
                    result.sq_distance <= gatherer.width * gatherer.width) {
                    events.push_back({
                        item_idx,
                        gatherer_idx,
                        result.sq_distance,
                        result.proj_ratio
                        });
                }
            }
        }

        // Сортируем события по времени (proj_ratio)
        std::sort(events.begin(), events.end(), [](const GatheringEvent& e1, const GatheringEvent& e2) {
            return e1.time < e2.time;
            });

        return events;
    }

}  // namespace collision_detector