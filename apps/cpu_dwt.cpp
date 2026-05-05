#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "lifting/cpu_transform.hpp"
#include "lifting/executor.hpp"
#include "lifting/scheme.hpp"

namespace {

struct Options {
    std::filesystem::path scheme_path;
    std::string signal_csv;
    std::string dtype{"float"};
    ttwv::cpu::ExecutionPolicy policy{ttwv::cpu::ExecutionPolicy::kSerial};
    size_t parallel_threshold{4096};
    int threads{0};
};

[[noreturn]] void usage(const char* program, const int exit_code) {
    std::cout << "Usage: " << program << " --scheme PATH --signal CSV [options]\n"
              << "\nOptions:\n"
              << "  --dtype float|double          Transform scalar type (default: float).\n"
              << "  --policy serial|openmp        Execution policy (default: serial).\n"
              << "  --parallel-threshold N        Minimum loop size for parallel execution (default: 4096).\n"
              << "  --threads N                   OpenMP thread count, 0 means runtime default.\n"
              << "  -h, --help                    Show this help message.\n";
    std::exit(exit_code);
}

[[nodiscard]] size_t parse_size(const std::string& value, const char* label) {
    size_t parsed_chars = 0;
    const size_t parsed = std::stoull(value, &parsed_chars);
    if (parsed_chars != value.size()) {
        throw std::runtime_error(std::string("invalid ") + label + ": " + value);
    }
    return parsed;
}

[[nodiscard]] int parse_int(const std::string& value, const char* label) {
    size_t parsed_chars = 0;
    const int parsed = std::stoi(value, &parsed_chars);
    if (parsed_chars != value.size()) {
        throw std::runtime_error(std::string("invalid ") + label + ": " + value);
    }
    return parsed;
}

[[nodiscard]] Options parse_args(const int argc, char** argv) {
    Options options;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            usage(argv[0], EXIT_SUCCESS);
        }

        auto require_value = [&](const char* label) -> std::string {
            if (i + 1 >= argc) {
                throw std::runtime_error(std::string(label) + " expects a value");
            }
            return argv[++i];
        };

        if (arg == "--scheme") {
            options.scheme_path = require_value("--scheme");
        } else if (arg == "--signal") {
            options.signal_csv = require_value("--signal");
        } else if (arg == "--dtype") {
            options.dtype = require_value("--dtype");
            if (options.dtype != "float" && options.dtype != "double") {
                throw std::runtime_error("--dtype must be 'float' or 'double'");
            }
        } else if (arg == "--policy") {
            const std::string policy = require_value("--policy");
            if (policy == "serial") {
                options.policy = ttwv::cpu::ExecutionPolicy::kSerial;
            } else if (policy == "openmp") {
                options.policy = ttwv::cpu::ExecutionPolicy::kOpenMP;
            } else {
                throw std::runtime_error("--policy must be 'serial' or 'openmp'");
            }
        } else if (arg == "--parallel-threshold") {
            options.parallel_threshold = parse_size(require_value("--parallel-threshold"), "--parallel-threshold");
        } else if (arg == "--threads") {
            options.threads = parse_int(require_value("--threads"), "--threads");
        } else {
            throw std::runtime_error("unknown argument: " + arg);
        }
    }

    if (options.scheme_path.empty()) {
        throw std::runtime_error("--scheme is required");
    }
    if (options.signal_csv.empty()) {
        throw std::runtime_error("--signal is required");
    }

    return options;
}

template <class T>
[[nodiscard]] std::vector<T> parse_signal_csv(const std::string& raw) {
    std::vector<T> values;
    std::string token;
    std::stringstream stream(raw);

    while (std::getline(stream, token, ',')) {
        if (token.empty()) {
            continue;
        }
        size_t parsed_chars = 0;
        const double value = std::stod(token, &parsed_chars);
        if (parsed_chars != token.size()) {
            throw std::runtime_error("invalid signal value: " + token);
        }
        values.push_back(static_cast<T>(value));
    }

    if (values.empty()) {
        throw std::runtime_error("signal must contain at least one value");
    }
    return values;
}

template <class T>
void print_array(const std::vector<T>& values) {
    std::cout << '[';
    for (size_t i = 0; i < values.size(); ++i) {
        if (i > 0) {
            std::cout << ',';
        }
        std::cout << static_cast<long double>(values[i]);
    }
    std::cout << ']';
}

template <class T>
void run_transform(const Options& options) {
    const auto scheme = ttwv::cpu::load_lifting_scheme_json(options.scheme_path);
    const std::vector<T> signal = parse_signal_csv<T>(options.signal_csv);
    const ttwv::cpu::Executor executor(options.policy, options.parallel_threshold, options.threads);
    const auto result =
        ttwv::cpu::dwt_forward_one_level<T>(std::span<const T>(signal.data(), signal.size()), scheme, executor);

    std::cout << std::setprecision(17);
    std::cout << "{\"cA\":";
    print_array(result.approximation);
    std::cout << ",\"cD\":";
    print_array(result.detail);
    std::cout << "}\n";
}

}  // namespace

int main(int argc, char** argv) {
    try {
        const Options options = parse_args(argc, argv);
        if (options.dtype == "double") {
            run_transform<double>(options);
        } else {
            run_transform<float>(options);
        }
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n';
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
