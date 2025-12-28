#pragma once
#include <random>
#include <string>
#include <sstream>
#include <iomanip>



namespace detail {
    struct TokenTag {};
}  // namespace detail

using Token = util::Tagged<std::string, detail::TokenTag>;

class TokenGenerator {
private:
    std::random_device random_device_;
    std::mt19937_64 generator1_;
    std::mt19937_64 generator2_;

public:
    TokenGenerator()
        : generator1_{ [this] {
            std::uniform_int_distribution<std::mt19937_64::result_type> dist;
            return dist(random_device_);
        }() }
        , generator2_{ [this] {
            std::uniform_int_distribution<std::mt19937_64::result_type> dist;
            return dist(random_device_);
        }() } {
    }

    Token GenerateToken() {
        // Генерируем два 64-битных числа
        uint64_t part1 = generator1_();
        uint64_t part2 = generator2_();

        // Конвертируем в hex-строку
        std::stringstream ss;
        ss << std::hex << std::setfill('0')
            << std::setw(16) << part1
            << std::setw(16) << part2;

        std::string token_str = ss.str();

        // Проверяем, что строка имеет ровно 32 символа
        if (token_str.length() > 32) {
            token_str = token_str.substr(0, 32);
        }
        else if (token_str.length() < 32) {
            token_str = std::string(32 - token_str.length(), '0') + token_str;
        }

        return Token{ token_str };
    }
};