#include "args.h"
#include <iostream>
#include <string>
#include <vector>
#include <algorithm>

Args ParseCommandLine(int argc, const char* const argv[]) {
    Args args;
    std::vector<std::string> arguments(argv + 1, argv + argc);

    auto get_next_arg = [&](size_t& i) -> std::string {
        if (i + 1 >= arguments.size()) {
            std::cerr << "Error: Missing value for option " << arguments[i] << "\n";
            exit(EXIT_FAILURE);
        }
        return arguments[++i];
        };

    for (size_t i = 0; i < arguments.size(); ++i) {
        const std::string& arg = arguments[i];

        if (arg == "--help" || arg == "-h") {
            std::cout << "Allowed options:\n"
                << "  -h [ --help ]          produce help message\n"
                << "  -t [ --tick-period ]   set tick period (milliseconds)\n"
                << "  -c [ --config-file ]   set config file path (required)\n"
                << "  -w [ --www-root ]      set static files root\n"
                << "  --randomize-spawn-points spawn dogs at random positions\n";
            exit(EXIT_SUCCESS);
        }
        else if (arg == "--tick-period" || arg == "-t") {
            std::string value = get_next_arg(i);
            try {
                args.tick_period = std::stoi(value);
            }
            catch (const std::exception&) {
                std::cerr << "Error: Invalid tick period value: " << value << "\n";
                exit(EXIT_FAILURE);
            }
        }
        else if (arg == "--config-file" || arg == "-c") {
            args.config_file = get_next_arg(i);
        }
        else if (arg == "--www-root" || arg == "-w") {
            args.www_root = get_next_arg(i);
        }
        else if (arg == "--randomize-spawn-points") {
            args.randomize_spawn_points = true;
        }
        else {
            std::cerr << "Error: Unknown option: " << arg << "\n";
            exit(EXIT_FAILURE);
        }
    }

    if (args.config_file.empty()) {
        std::cerr << "Error: Config file is required\n";
        exit(EXIT_FAILURE);
    }

    return args;
}