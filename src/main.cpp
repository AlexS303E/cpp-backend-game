#include <boost/asio/signal_set.hpp>
#include <boost/asio/io_context.hpp>
#include <chrono>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>
#include <memory>
#include <cstdlib>
#include <string_view>

#include "sdk.h"
#include "json_loader.h"
#include "request_handler.h"
#include "http_server.h"
#include "token.h"
#include "args.h"
#include "serializing_listener.h"
#include "record_repository.h"

using namespace std::literals;
namespace net = boost::asio;
namespace sys = boost::system;

const auto address = net::ip::make_address("0.0.0.0");
constexpr net::ip::port_type port = 8080;

namespace {
    template <typename Fn>
    void RunWorkers(unsigned n, const Fn& fn) {
        n = std::max(1u, n);
        std::vector<std::jthread> workers;
        workers.reserve(n - 1);
        while (--n) {
            workers.emplace_back(fn);
        }
        fn();
    }
}

std::string GetDbUrlFromEnv() {
    if (const char* env = std::getenv("GAME_DB_URL")) {
        return std::string(env);
    }
    throw std::runtime_error("GAME_DB_URL is not set");
}

int main(int argc, const char* argv[]) {
    auto args = ParseCommandLine(argc, argv);

    try {
        auto game_ptr = json_loader::LoadGame(args.config_file);
        auto& game = *game_ptr;

        std::unique_ptr<app::SerializingListener> serializing_listener;
        if (!args.state_file.empty()) {
            serializing_listener = std::make_unique<app::SerializingListener>(
                game,
                args.state_file,
                std::chrono::milliseconds(args.save_state_period > 0 ? args.save_state_period : 0)
            );

            serializing_listener->LoadState();
        }

        bool game_loop_started = false;
        if (args.tick_period > 0) {
            game.SetTickPeriod(args.tick_period);
            if (args.tick_period > 0) {
                game.StartGameLoop();
                game_loop_started = true;
            }
            std::cout << "Game loop started..."sv << std::endl;
        }

        auto db_url = GetDbUrlFromEnv();
        auto records = std::make_shared<RecordRepository>(db_url);

        game.SetRetiredPlayerCallback([records](const model::Player& player) {
            try {
                // Получаем данные игрока
                std::string name = player.GetDog().GetName();
                int score = player.GetScore();
                double play_time = player.GetPlayTime(); // уже в секундах

                // Сохраняем в базу данных
                records->AddRecord(name, score, play_time);

                std::cout << "Player retired and saved to DB: "
                    << name << ", score: " << score
                    << ", play time: " << play_time << "s" << std::endl;
            }
            catch (const std::exception& e) {
                std::cerr << "Failed to save retired player record: "
                    << e.what() << std::endl;
            }
            });

        const unsigned num_threads = std::thread::hardware_concurrency();
        net::io_context ioc(num_threads);

        net::signal_set signals(ioc, SIGINT, SIGTERM);
        signals.async_wait(
            [&ioc, &game, &serializing_listener, &game_loop_started]
            (const sys::error_code& ec, int) {
                if (!ec) {
                    std::cout << "Shutting down server..."sv << std::endl;
                    if (serializing_listener) serializing_listener->SaveNow();
                    if (game_loop_started) game.StopGameLoop();
                    ioc.stop();
                }
            });

        auto api_strand = net::make_strand(ioc);

        auto handler = std::make_shared<http_handler::RequestHandler>(
            game,
            api_strand,
            args.www_root,
            args.tick_period == 0,
            args.randomize_spawn_points,
            serializing_listener.get(),
            records
        );

        http_server::ServeHttp(ioc, { address, port },
            [handler](auto&& req, auto&& send) {
                (*handler)(std::forward<decltype(req)>(req),
                    std::forward<decltype(send)>(send));
            });

        std::cout << "Server has started on port " << port << "..."sv << std::endl;

        if (args.save_state_period > 0) {
            std::cout << "Game state will be auto-saved to: "
                << args.state_file << std::endl;
        }

        std::cout << "Press Ctrl+C to exit..."sv << std::endl;

        RunWorkers(std::max(1u, num_threads), [&ioc] { ioc.run(); });

        std::cout << "Server stopped successfully."sv << std::endl;
    }
    catch (const std::exception& ex) {
        std::cerr << ex.what() << std::endl;
        return EXIT_FAILURE;
    }

    return 0;
}

