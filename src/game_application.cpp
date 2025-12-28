#include "game_application.h"
#include <chrono>
#include <thread>
#include <algorithm>

namespace app {

void Application::AddListener(std::shared_ptr<ApplicationListener> listener) {
    listeners_.push_back(listener);
}

void Application::RemoveListener(std::shared_ptr<ApplicationListener> listener) {
    listeners_.erase(
        std::remove(listeners_.begin(), listeners_.end(), listener),
        listeners_.end()
    );
}

void Application::Tick(std::chrono::milliseconds delta) {
    // Уведомляем всех слушателей о тике
    for (auto& listener : listeners_) {
        listener->OnTick(delta);
    }
    
    // Обновляем состояние игры
    game_.UpdateState(delta.count() / 1000.0);
}

void Application::StartGameLoop() {
    if (game_loop_running_ || tick_period_.count() == 0) {
        return;
    }
    
    game_loop_running_ = true;
    game_loop_thread_ = std::thread([this]() { GameLoop(); });
}

void Application::StopGameLoop() {
    game_loop_running_ = false;
    if (game_loop_thread_.joinable()) {
        game_loop_thread_.join();
    }
}

void Application::SetTickPeriod(std::chrono::milliseconds period) {
    tick_period_ = period;
    game_.SetTickPeriod(period.count() * 1000); // конвертируем в микросекунды
}

void Application::GameLoop() {
    using namespace std::chrono;
    auto last_tick_time = steady_clock::now();

    while (game_loop_running_) {
        auto current_time = steady_clock::now();
        auto delta = duration_cast<milliseconds>(current_time - last_tick_time);
        last_tick_time = current_time;

        Tick(delta);

        // Спим до следующего тика
        if (tick_period_.count() > 0) {
            auto elapsed = steady_clock::now() - current_time;
            auto sleep_duration = tick_period_ - duration_cast<milliseconds>(elapsed);
            if (sleep_duration > milliseconds(0)) {
                std::this_thread::sleep_for(sleep_duration);
            }
        }
    }
}

} // namespace app