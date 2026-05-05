#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <random>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

#include "lifting/cpu_transform.hpp"
#include "lifting/executor.hpp"
#include "lifting/plan.hpp"
#include "lifting/scheme.hpp"

namespace {

struct Options {
    std::filesystem::path scheme_path;
    std::string signal_kind{"ramp"};
    std::string dtype{"float"};
    ttwv::cpu::ExecutionPolicy policy{ttwv::cpu::ExecutionPolicy::kSerial};
    size_t start_size{100000};
    size_t end_size{1000000};
    size_t step_size{10000};
    size_t repeats{5};
    size_t warmups{1};
    size_t parallel_threshold{4096};
    int threads{0};
    double magnitude{1.0};
    uint32_t seed{2};
};

[[noreturn]] void usage(const char* program, const int exit_code) {
    std::cout << "Usage: " << program << " --scheme PATH [options]\n"
              << "\nOptions:\n"
              << "  --kind NAME                 Signal kind: uniform, normal, alternating, impulse, ramp,\n"
              << "                              sinusoidal, constant, step, square (default: ramp).\n"
              << "  --dtype float|double        Transform scalar type (default: float).\n"
              << "  --policy serial|openmp      Executor policy (default: serial).\n"
              << "  --threads N                 OpenMP thread count, 0 means runtime default.\n"
              << "  --parallel-threshold N      Minimum loop size for parallel execution (default: 4096).\n"
              << "  --start N                   First signal size (default: 100000).\n"
              << "  --end N                     Last signal size, inclusive (default: 1000000).\n"
              << "  --step N                    Signal size step (default: 10000).\n"
              << "  --repeats N                 Timed repeats per size (default: 5).\n"
              << "  --warmups N                 Untimed warmups per size (default: 1).\n"
              << "  --magnitude V               Signal magnitude (default: 1.0).\n"
              << "  --seed N                    Seed for stochastic signals (default: 2).\n"
              << "  -h, --help                  Show this help message.\n";
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

[[nodiscard]] double parse_double(const std::string& value, const char* label) {
    size_t parsed_chars = 0;
    const double parsed = std::stod(value, &parsed_chars);
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
        } else if (arg == "--kind") {
            options.signal_kind = require_value("--kind");
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
        } else if (arg == "--threads") {
            options.threads = parse_int(require_value("--threads"), "--threads");
        } else if (arg == "--parallel-threshold") {
            options.parallel_threshold = parse_size(require_value("--parallel-threshold"), "--parallel-threshold");
        } else if (arg == "--start") {
            options.start_size = parse_size(require_value("--start"), "--start");
        } else if (arg == "--end") {
            options.end_size = parse_size(require_value("--end"), "--end");
        } else if (arg == "--step") {
            options.step_size = parse_size(require_value("--step"), "--step");
        } else if (arg == "--repeats") {
            options.repeats = parse_size(require_value("--repeats"), "--repeats");
        } else if (arg == "--warmups") {
            options.warmups = parse_size(require_value("--warmups"), "--warmups");
        } else if (arg == "--magnitude") {
            options.magnitude = parse_double(require_value("--magnitude"), "--magnitude");
        } else if (arg == "--seed") {
            options.seed = static_cast<uint32_t>(parse_size(require_value("--seed"), "--seed"));
        } else {
            throw std::runtime_error("unknown argument: " + arg);
        }
    }

    if (options.scheme_path.empty()) {
        throw std::runtime_error("--scheme is required");
    }
    if (options.start_size == 0 || options.end_size == 0 || options.step_size == 0) {
        throw std::runtime_error("--start/--end/--step must be positive");
    }
    if (options.end_size < options.start_size) {
        throw std::runtime_error("--end must be greater than or equal to --start");
    }
    if (options.repeats == 0) {
        throw std::runtime_error("--repeats must be positive");
    }

    return options;
}

[[nodiscard]] std::string stem_string(const std::filesystem::path& path) { return path.stem().string(); }

template <class T>
[[nodiscard]] std::vector<T> generate_signal(
    const std::string& kind,
    const size_t length,
    const double magnitude,
    const uint32_t seed) {
    std::vector<T> out(length);
    const T mag = static_cast<T>(magnitude);

    if (kind == "uniform") {
        std::mt19937_64 rng(seed);
        std::uniform_real_distribution<double> dist(-magnitude, magnitude);
        for (T& value : out) {
            value = static_cast<T>(dist(rng));
        }
        return out;
    }

    if (kind == "normal") {
        std::mt19937_64 rng(seed);
        std::normal_distribution<double> dist(0.0, magnitude);
        for (T& value : out) {
            value = static_cast<T>(dist(rng));
        }
        return out;
    }

    if (kind == "alternating") {
        for (size_t i = 0; i < length; ++i) {
            out[i] = (i & 1U) == 0 ? mag : -mag;
        }
        return out;
    }

    if (kind == "impulse") {
        std::fill(out.begin(), out.end(), T{0});
        out[length / 2] = mag;
        return out;
    }

    if (kind == "ramp") {
        if (length == 1) {
            out[0] = -mag;
            return out;
        }
        for (size_t i = 0; i < length; ++i) {
            const double t = static_cast<double>(i) / static_cast<double>(length - 1);
            out[i] = static_cast<T>((2.0 * t - 1.0) * magnitude);
        }
        return out;
    }

    if (kind == "sinusoidal") {
        constexpr double pi = 3.141592653589793238462643383279502884;
        for (size_t i = 0; i < length; ++i) {
            out[i] = static_cast<T>(magnitude * std::sin(2.0 * pi * 3.0 * static_cast<double>(i) / length));
        }
        return out;
    }

    if (kind == "constant") {
        std::fill(out.begin(), out.end(), mag);
        return out;
    }

    if (kind == "step") {
        std::fill(out.begin(), out.end(), T{0});
        std::fill(out.begin() + static_cast<std::ptrdiff_t>(length / 2), out.end(), mag);
        return out;
    }

    if (kind == "square") {
        constexpr double pi = 3.141592653589793238462643383279502884;
        for (size_t i = 0; i < length; ++i) {
            const double phase = std::sin(2.0 * pi * 3.0 * static_cast<double>(i) / length);
            out[i] = phase >= 0.0 ? mag : -mag;
        }
        return out;
    }

    throw std::runtime_error("unsupported signal kind: " + kind);
}

template <class T>
[[nodiscard]] double checksum(const ttwv::cpu::DwtResult<T>& result) {
    long double sum = 0.0;
    for (const T value : result.approximation) {
        sum += static_cast<long double>(value);
    }
    for (const T value : result.detail) {
        sum += static_cast<long double>(value);
    }
    return static_cast<double>(sum);
}

[[nodiscard]] double mean(const std::vector<double>& values) {
    double sum = 0.0;
    for (const double value : values) {
        sum += value;
    }
    return sum / static_cast<double>(values.size());
}

template <class T>
void run_benchmark(const Options& options) {
    const auto scheme = ttwv::cpu::load_lifting_scheme_json(options.scheme_path);
    const ttwv::cpu::Executor executor(options.policy, options.parallel_threshold, options.threads);
    const std::string wavelet = stem_string(options.scheme_path);
    const std::string policy_name = options.policy == ttwv::cpu::ExecutionPolicy::kOpenMP ? "openmp" : "serial";

    std::cout << "wavelet,signal_kind,size,dtype,policy,threads,parallel_threshold,repeats,warmups,"
                 "mean_ms,min_ms,max_ms,checksum\n";

    for (size_t size = options.start_size; size <= options.end_size; size += options.step_size) {
        const std::vector<T> signal = generate_signal<T>(options.signal_kind, size, options.magnitude, options.seed);
        const auto plan = ttwv::cpu::build_forward_plan(scheme, signal.size());
        auto workspace = ttwv::cpu::allocate_workspace<T>(plan);

        double last_checksum = 0.0;
        for (size_t warmup = 0; warmup < options.warmups; ++warmup) {
            const auto result = ttwv::cpu::dwt_forward_one_level<T>(
                std::span<const T>(signal.data(), signal.size()), scheme, plan, workspace, executor);
            last_checksum += checksum(result);
        }

        std::vector<double> times_ms;
        times_ms.reserve(options.repeats);
        for (size_t repeat = 0; repeat < options.repeats; ++repeat) {
            const auto start = std::chrono::steady_clock::now();
            const auto result = ttwv::cpu::dwt_forward_one_level<T>(
                std::span<const T>(signal.data(), signal.size()), scheme, plan, workspace, executor);
            const auto end = std::chrono::steady_clock::now();
            last_checksum += checksum(result);

            const auto elapsed = std::chrono::duration<double, std::milli>(end - start).count();
            times_ms.push_back(elapsed);
        }

        const auto [min_it, max_it] = std::minmax_element(times_ms.begin(), times_ms.end());
        std::cout << wavelet << ',' << options.signal_kind << ',' << size << ','
                  << (std::is_same_v<T, double> ? "double" : "float") << ',' << policy_name << ','
                  << options.threads << ',' << options.parallel_threshold << ',' << options.repeats << ','
                  << options.warmups << ',' << std::setprecision(10) << mean(times_ms) << ',' << *min_it << ','
                  << *max_it << ',' << last_checksum << '\n';

        if (size > options.end_size - options.step_size) {
            break;
        }
    }
}

}  // namespace

int main(int argc, char** argv) {
    try {
        const Options options = parse_args(argc, argv);
        if (options.dtype == "double") {
            run_benchmark<double>(options);
        } else {
            run_benchmark<float>(options);
        }
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n';
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
