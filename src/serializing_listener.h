#pragma once

#include "application_listener.h"
#include "state_serializer.h"
#include <chrono>
#include <filesystem>

namespace app {

    class SerializingListener : public ApplicationListener {
    public:
        SerializingListener(model::Game& game,
            const std::filesystem::path& state_file,
            std::chrono::milliseconds save_period);

        void OnTick(std::chrono::milliseconds delta) override;

        // Метод для загрузки состояния при старте
        void LoadState();

        void SaveNow();

    private:
        model::Game& game_;
        std::filesystem::path state_file_;
        std::chrono::milliseconds save_period_;
        std::chrono::milliseconds time_since_last_save_{ 0 };
        state_serializer::StateSerializer serializer_;
    };

} // namespace app