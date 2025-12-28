#include "serializing_listener.h"
#include <iostream>

namespace app {

    SerializingListener::SerializingListener(model::Game& game,
        const std::filesystem::path& state_file,
        std::chrono::milliseconds save_period)
        : game_(game)
        , state_file_(state_file)
        , save_period_(save_period) {
    }

    void SerializingListener::OnTick(std::chrono::milliseconds delta) {
        time_since_last_save_ += delta;

        if (time_since_last_save_ >= save_period_) {
            try {
                serializer_.Serialize(game_, state_file_);
                std::cout << "Auto-saved game state to: " << state_file_ << std::endl;
                time_since_last_save_ = std::chrono::milliseconds(0);
            }
            catch (const std::exception& ex) {
                std::cerr << "Failed to auto-save game state: " << ex.what() << std::endl;
            }
        }
    }

    void SerializingListener::SaveNow() {
        try {
            serializer_.Serialize(game_, state_file_);
            std::cout << "Game state saved to: " << state_file_ << std::endl;
        }
        catch (const std::exception& ex) {
            std::cerr << "Failed to save game state: " << ex.what() << std::endl;
        }
    }

    // Метод для загрузки состояния при старте
    void SerializingListener::LoadState() {
        try {
            serializer_.Deserialize(game_, state_file_);
            std::cout << "Loaded game state from: " << state_file_ << std::endl;
        }
        catch (const std::exception& ex) {
            std::cout << "No saved state found or error loading: " << ex.what() << std::endl;
        }
    }

} // namespace app