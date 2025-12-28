#pragma once
#include <chrono>

namespace app {

    class ApplicationListener {
    public:
        virtual ~ApplicationListener() = default;
        virtual void OnTick(std::chrono::milliseconds delta) = 0;
    };

} // namespace app