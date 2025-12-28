#pragma once
#include "application_listener.h"
#include "model.h"
#include <boost/signals2.hpp>
#include <chrono>
#include <vector>
#include <memory>

namespace app {

class Application {
public:
    using TickSignal = boost::signals2::signal<void(std::chrono::milliseconds delta)>;

    Application() = default;
    
    // ���������� �����������
    void AddListener(std::shared_ptr<ApplicationListener> listener);
    void RemoveListener(std::shared_ptr<ApplicationListener> listener);
    
    // ����������� � ����
    void Tick(std::chrono::milliseconds delta);
    
    // ��������� ������� ������
    model::Game& GetGame() { return game_; }
    const model::Game& GetGame() const { return game_; }
    
    // ���������� ������� ������
    void StartGameLoop();
    void StopGameLoop();
    void SetTickPeriod(std::chrono::milliseconds period);

private:
    void GameLoop();
    
    model::Game game_;
    std::vector<std::shared_ptr<ApplicationListener>> listeners_;
    std::atomic<bool> game_loop_running_{false};
    std::thread game_loop_thread_;
    std::chrono::milliseconds tick_period_{0};
};

} // namespace app