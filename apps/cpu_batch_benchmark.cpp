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
    ttwv::cpu::ExecutionPolicy batch_policy{ttwv::cpu::ExecutionPolicy::kOpenMP};
    ttwv::cpu::ExecutionPolicy inner_policy{ttwv::cpu::ExecutionPolicy::kSerial};
    size_t start_size{1024};
    size_t end_size{65536};
    size_t step_size{1024};
    size_t batch_size{64};
    size_t repeats{5};
    size_t warmups{1};
    size_t stencil_threshold{ttwv::cpu::kDefaultStencilParallelThreshold};
    size_t scale_threshold{ttwv::cpu::kDefaultScaleParallelThreshold};
    size_t batch_threshold{ttwv::cpu::kDefaultBatchParallelThreshold};
    int batch_threads{0};
    int inner_threads{0};
    double magnitude{1.0};
    uint32_t seed{2};
};

[[noreturn]] void usage(const char* program, const int exit_code) {
    std::cout << "Usage: " << program << " --scheme PATH [options]\n"
              << "\nOptions:\n"
              << "  --kind NAME                    Signal kind: uniform, normal, alternating, impulse,\n"
              << "                                 ramp, sinusoidal, constant, step, square.\n"
              << "  --dtype float|double           Transform scalar type (default: float).\n"
              << "  --batch-policy serial|openmp   Outer batch executor policy (default: openmp).\n"
              << "  --inner-policy serial|openmp   Per-signal executor policy (default: serial).\n"
              << "  --batch-threads N              OpenMP thread count for outer batch executor.\n"
              << "  --inner-threads N              OpenMP thread count for per-signal executor.\n"
              << "  --batch-threshold N            Minimum number of signals for batch parallelism.\n"
              << "  --parallel-threshold N         Alias for --stencil-threshold.\n"
              << "  --stencil-threshold N          Minimum stencil/split loop size for inner parallelism.\n"
              << "  --scale-threshold N            Minimum scale loop size for inner parallelism.\n"
              << "  --batch-size N                 Number of independent signals per timed batch.\n"
              << "  --start N                      First per-signal size (default: 1024).\n"
              << "  --end N                        Last per-signal size, inclusive (default: 65536).\n"
              << "  --step N                       Per-signal size step (default: 1024).\n"
              << "  --repeats N                    Timed repeats per size (default: 5).\n"
              << "  --warmups N                    Untimed warmups per size (default: 1).\n"
              << "  --magnitude V                  Signal magnitude (default: 1.0).\n"
              << "  --seed N                       Seed for stochastic signals (default: 2).\n"
              << "  -h, --help                     Show this help message.\n";
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

[[nodiscard]] ttwv::cpu::ExecutionPolicy parse_policy(const std::string& raw, const char* label) {
    if (raw == "serial") {
        return ttwv::cpu::ExecutionPolicy::kSerial;
    }
    if (raw == "openmp") {
        return ttwv::cpu::ExecutionPolicy::kOpenMP;
    }
    throw std::runtime_error(std::string(label) + " must be 'serial' or 'openmp'");
}

[[nodiscard]] const char* policy_name(const ttwv::cpu::ExecutionPolicy policy) {
    return policy == ttwv::cpu::ExecutionPolicy::kOpenMP ? "openmp" : "serial";
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
        } else if (arg == "--batch-policy") {
            options.batch_policy = parse_policy(require_value("--batch-policy"), "--batch-policy");
        } else if (arg == "--inner-policy") {
            options.inner_policy = parse_policy(require_value("--inner-policy"), "--inner-policy");
        } else if (arg == "--batch-threads") {
            options.batch_threads = parse_int(require_value("--batch-threads"), "--batch-threads");
        } else if (arg == "--inner-threads") {
            options.inner_threads = parse_int(require_value("--inner-threads"), "--inner-threads");
        } else if (arg == "--batch-threshold") {
            options.batch_threshold = parse_size(require_value("--batch-threshold"), "--batch-threshold");
        } else if (arg == "--parallel-threshold" || arg == "--stencil-threshold") {
            options.stencil_threshold = parse_size(require_value(arg.c_str()), arg.c_str());
        } else if (arg == "--scale-threshold") {
            options.scale_threshold = parse_size(require_value("--scale-threshold"), "--scale-threshold");
        } else if (arg == "--batch-size") {
            options.batch_size = parse_size(require_value("--batch-size"), "--batch-size");
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
    if (options.start_size == 0 || options.end_size == 0 || options.step_size == 0 || options.batch_size == 0) {
        throw std::runtime_error("--start/--end/--step/--batch-size must be positive");
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
[[nodiscard]] double checksum(const ttwv::cpu::DwtBatchResult<T>& result) {
    long double sum = 0.0;
    for (const auto& signal : result.signals) {
        for (const T value : signal.approximation) {
            sum += static_cast<long double>(value);
        }
        for (const T value : signal.detail) {
            sum += static_cast<long double>(value);
        }
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
    const ttwv::cpu::Executor batch_executor(
        options.batch_policy,
        options.stencil_threshold,
        options.batch_threads,
        options.scale_threshold,
        options.batch_threshold);
    const ttwv::cpu::Executor signal_executor(
        options.inner_policy,
        options.stencil_threshold,
        options.inner_threads,
        options.scale_threshold,
        options.batch_threshold);
    const std::string wavelet = stem_string(options.scheme_path);

    std::cout << "wavelet,signal_kind,signal_size,batch_size,total_samples,dtype,batch_policy,batch_threads,"
                 "inner_policy,inner_threads,parallel_threshold,scale_threshold,batch_threshold,repeats,warmups,"
                 "mean_ms,min_ms,max_ms,checksum\n";

    for (size_t size = options.start_size; size <= options.end_size; size += options.step_size) {
        std::vector<std::vector<T>> signals;
        signals.reserve(options.batch_size);
        for (size_t i = 0; i < options.batch_size; ++i) {
            signals.push_back(generate_signal<T>(
                options.signal_kind,
                size,
                options.magnitude,
                static_cast<uint32_t>(options.seed + i)));
        }

        std::vector<std::span<const T>> spans;
        spans.reserve(signals.size());
        for (const auto& signal : signals) {
            spans.emplace_back(signal.data(), signal.size());
        }

        auto workspace = ttwv::cpu::allocate_batch_workspace<T>(
            std::span<const std::span<const T>>(spans.data(), spans.size()),
            scheme);

        double last_checksum = 0.0;
        for (size_t warmup = 0; warmup < options.warmups; ++warmup) {
            const auto result = ttwv::cpu::dwt_forward_one_level_batch<T>(
                std::span<const std::span<const T>>(spans.data(), spans.size()),
                scheme,
                workspace,
                batch_executor,
                signal_executor);
            last_checksum += checksum(result);
        }

        std::vector<double> times_ms;
        times_ms.reserve(options.repeats);
        for (size_t repeat = 0; repeat < options.repeats; ++repeat) {
            const auto start = std::chrono::steady_clock::now();
            const auto result = ttwv::cpu::dwt_forward_one_level_batch<T>(
                std::span<const std::span<const T>>(spans.data(), spans.size()),
                scheme,
                workspace,
                batch_executor,
                signal_executor);
            const auto end = std::chrono::steady_clock::now();
            last_checksum += checksum(result);

            const auto elapsed = std::chrono::duration<double, std::milli>(end - start).count();
            times_ms.push_back(elapsed);
        }

        const auto [min_it, max_it] = std::minmax_element(times_ms.begin(), times_ms.end());
        std::cout << wavelet << ',' << options.signal_kind << ',' << size << ',' << options.batch_size << ','
                  << size * options.batch_size << ',' << (std::is_same_v<T, double> ? "double" : "float") << ','
                  << policy_name(options.batch_policy) << ',' << options.batch_threads << ','
                  << policy_name(options.inner_policy) << ',' << options.inner_threads << ','
                  << options.stencil_threshold << ',' << options.scale_threshold << ',' << options.batch_threshold
                  << ',' << options.repeats << ',' << options.warmups << ',' << std::setprecision(10)
                  << mean(times_ms) << ',' << *min_it << ',' << *max_it << ',' << last_checksum << '\n';

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
