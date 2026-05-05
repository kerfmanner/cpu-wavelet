#include "lifting/scheme.hpp"

#include <cctype>
#include <cstdlib>
#include <fstream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace ttwv::cpu {

namespace {

struct JsonValue {
    enum class Kind {
        kNull,
        kBool,
        kNumber,
        kString,
        kArray,
        kObject,
    };

    Kind kind{Kind::kNull};
    bool bool_value{false};
    double number_value{0.0};
    std::string string_value;
    std::vector<JsonValue> array_value;
    std::map<std::string, JsonValue> object_value;
};

class JsonParser {
   public:
    explicit JsonParser(std::string text) : text_(std::move(text)) {}

    [[nodiscard]] JsonValue parse() {
        JsonValue value = parse_value();
        skip_ws();
        if (pos_ != text_.size()) {
            fail("unexpected trailing content");
        }
        return value;
    }

   private:
    [[nodiscard]] JsonValue parse_value() {
        skip_ws();
        if (pos_ >= text_.size()) {
            fail("unexpected end of JSON");
        }

        const char ch = text_[pos_];
        if (ch == '{') {
            return parse_object();
        }
        if (ch == '[') {
            return parse_array();
        }
        if (ch == '"') {
            JsonValue value;
            value.kind = JsonValue::Kind::kString;
            value.string_value = parse_string();
            return value;
        }
        if (ch == '-' || std::isdigit(static_cast<unsigned char>(ch)) != 0) {
            return parse_number();
        }
        if (consume_literal("true")) {
            JsonValue value;
            value.kind = JsonValue::Kind::kBool;
            value.bool_value = true;
            return value;
        }
        if (consume_literal("false")) {
            JsonValue value;
            value.kind = JsonValue::Kind::kBool;
            value.bool_value = false;
            return value;
        }
        if (consume_literal("null")) {
            return JsonValue{};
        }

        fail("unexpected JSON value");
    }

    [[nodiscard]] JsonValue parse_object() {
        expect('{');
        JsonValue value;
        value.kind = JsonValue::Kind::kObject;
        skip_ws();
        if (consume('}')) {
            return value;
        }

        while (true) {
            skip_ws();
            if (pos_ >= text_.size() || text_[pos_] != '"') {
                fail("expected object key");
            }
            std::string key = parse_string();
            skip_ws();
            expect(':');
            value.object_value.emplace(std::move(key), parse_value());
            skip_ws();
            if (consume('}')) {
                return value;
            }
            expect(',');
        }
    }

    [[nodiscard]] JsonValue parse_array() {
        expect('[');
        JsonValue value;
        value.kind = JsonValue::Kind::kArray;
        skip_ws();
        if (consume(']')) {
            return value;
        }

        while (true) {
            value.array_value.push_back(parse_value());
            skip_ws();
            if (consume(']')) {
                return value;
            }
            expect(',');
        }
    }

    [[nodiscard]] std::string parse_string() {
        expect('"');
        std::string out;

        while (pos_ < text_.size()) {
            const char ch = text_[pos_++];
            if (ch == '"') {
                return out;
            }
            if (ch != '\\') {
                out.push_back(ch);
                continue;
            }
            if (pos_ >= text_.size()) {
                fail("unterminated escape sequence");
            }

            const char escaped = text_[pos_++];
            switch (escaped) {
                case '"': out.push_back('"'); break;
                case '\\': out.push_back('\\'); break;
                case '/': out.push_back('/'); break;
                case 'b': out.push_back('\b'); break;
                case 'f': out.push_back('\f'); break;
                case 'n': out.push_back('\n'); break;
                case 'r': out.push_back('\r'); break;
                case 't': out.push_back('\t'); break;
                default: fail("unsupported JSON string escape");
            }
        }

        fail("unterminated string");
    }

    [[nodiscard]] JsonValue parse_number() {
        const char* begin = text_.c_str() + pos_;
        char* end = nullptr;
        const double parsed = std::strtod(begin, &end);
        if (end == begin) {
            fail("invalid number");
        }

        pos_ = static_cast<size_t>(end - text_.c_str());
        JsonValue value;
        value.kind = JsonValue::Kind::kNumber;
        value.number_value = parsed;
        return value;
    }

    void skip_ws() noexcept {
        while (pos_ < text_.size() && std::isspace(static_cast<unsigned char>(text_[pos_])) != 0) {
            ++pos_;
        }
    }

    [[nodiscard]] bool consume(const char expected) noexcept {
        skip_ws();
        if (pos_ < text_.size() && text_[pos_] == expected) {
            ++pos_;
            return true;
        }
        return false;
    }

    [[nodiscard]] bool consume_literal(const std::string_view literal) noexcept {
        if (text_.compare(pos_, literal.size(), literal) != 0) {
            return false;
        }
        pos_ += literal.size();
        return true;
    }

    void expect(const char expected) {
        if (!consume(expected)) {
            std::string message = "expected '";
            message.push_back(expected);
            message.push_back('\'');
            fail(message);
        }
    }

    [[noreturn]] void fail(const std::string& message) const {
        throw std::runtime_error("JSON parse error at byte " + std::to_string(pos_) + ": " + message);
    }

    std::string text_;
    size_t pos_{0};
};

[[nodiscard]] const std::map<std::string, JsonValue>& as_object(const JsonValue& value, const char* label) {
    if (value.kind != JsonValue::Kind::kObject) {
        throw std::runtime_error(std::string(label) + " must be an object");
    }
    return value.object_value;
}

[[nodiscard]] const std::vector<JsonValue>& as_array(const JsonValue& value, const char* label) {
    if (value.kind != JsonValue::Kind::kArray) {
        throw std::runtime_error(std::string(label) + " must be an array");
    }
    return value.array_value;
}

[[nodiscard]] const std::string& as_string(const JsonValue& value, const char* label) {
    if (value.kind != JsonValue::Kind::kString) {
        throw std::runtime_error(std::string(label) + " must be a string");
    }
    return value.string_value;
}

[[nodiscard]] double as_number(const JsonValue& value, const char* label) {
    if (value.kind != JsonValue::Kind::kNumber) {
        throw std::runtime_error(std::string(label) + " must be a number");
    }
    return value.number_value;
}

[[nodiscard]] const JsonValue& required_field(
    const std::map<std::string, JsonValue>& object,
    const std::string& key,
    const char* label) {
    const auto it = object.find(key);
    if (it == object.end()) {
        throw std::runtime_error(std::string(label) + " is missing required field '" + key + "'");
    }
    return it->second;
}

[[nodiscard]] const JsonValue* optional_field(const std::map<std::string, JsonValue>& object, const std::string& key) {
    const auto it = object.find(key);
    return it == object.end() ? nullptr : &it->second;
}

[[nodiscard]] int as_int(const JsonValue& value, const char* label) {
    return static_cast<int>(as_number(value, label));
}

[[nodiscard]] StepType parse_step_type(const std::string& raw) {
    if (raw == "predict") {
        return StepType::kPredict;
    }
    if (raw == "update") {
        return StepType::kUpdate;
    }
    if (raw == "scale-even") {
        return StepType::kScaleEven;
    }
    if (raw == "scale-odd") {
        return StepType::kScaleOdd;
    }
    if (raw == "swap") {
        return StepType::kSwap;
    }
    throw std::runtime_error("unsupported lifting step type: " + raw);
}

[[nodiscard]] float parse_coefficient(const JsonValue& value) {
    if (value.kind == JsonValue::Kind::kNumber) {
        return static_cast<float>(value.number_value);
    }

    const auto& object = as_object(value, "coefficient");
    const double numerator = as_number(required_field(object, "numerator", "coefficient"), "coefficient.numerator");
    const double denominator =
        as_number(required_field(object, "denominator", "coefficient"), "coefficient.denominator");
    if (denominator == 0.0) {
        throw std::runtime_error("coefficient denominator must be non-zero");
    }
    return static_cast<float>(numerator / denominator);
}

void validate_scheme(const LiftingScheme& scheme) {
    if (scheme.mode != "symmetric") {
        throw std::runtime_error("only symmetric mode is supported by the CPU implementation");
    }
    if (scheme.tap_size <= 0) {
        throw std::runtime_error("tap_size must be positive");
    }
    if (scheme.steps.empty()) {
        throw std::runtime_error("lifting scheme must contain at least one step");
    }

    for (const auto& step : scheme.steps) {
        if ((step.type == StepType::kPredict || step.type == StepType::kUpdate) && step.coefficients.empty()) {
            throw std::runtime_error("predict/update steps must contain coefficients");
        }
        if ((step.type == StepType::kScaleEven || step.type == StepType::kScaleOdd) &&
            (step.shift != 0 || step.coefficients.size() != 1)) {
            throw std::runtime_error("scale steps must have zero shift and exactly one coefficient");
        }
        if (step.type == StepType::kSwap && !step.coefficients.empty()) {
            throw std::runtime_error("swap steps must not contain coefficients");
        }
    }
}

}  // namespace

LiftingScheme load_lifting_scheme_json(const std::filesystem::path& path) {
    std::ifstream file(path);
    if (!file.good()) {
        throw std::runtime_error("failed to open lifting scheme file: " + path.string());
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();

    JsonParser parser(buffer.str());
    const JsonValue root_value = parser.parse();
    const auto& root = as_object(root_value, "root");
    const auto& delay = as_object(required_field(root, "delay", "root"), "delay");
    const auto& steps = as_array(required_field(root, "steps", "root"), "steps");

    LiftingScheme scheme{
        .mode = "symmetric",
        .tap_size = as_int(required_field(root, "tap_size", "root"), "tap_size"),
        .delay_even = as_int(required_field(delay, "even", "delay"), "delay.even"),
        .delay_odd = as_int(required_field(delay, "odd", "delay"), "delay.odd"),
        .steps = {},
    };

    if (const JsonValue* mode = optional_field(root, "mode")) {
        scheme.mode = as_string(*mode, "mode");
    }

    scheme.steps.reserve(steps.size());
    for (size_t i = 0; i < steps.size(); ++i) {
        const auto& step_object = as_object(steps[i], "step");
        LiftingStep step{
            .type = parse_step_type(as_string(required_field(step_object, "type", "step"), "step.type")),
            .shift = 0,
            .coefficients = {},
        };

        if (const JsonValue* shift = optional_field(step_object, "shift")) {
            step.shift = as_int(*shift, "step.shift");
        }

        if (const JsonValue* coefficients = optional_field(step_object, "coefficients")) {
            const auto& coefficient_values = as_array(*coefficients, "step.coefficients");
            step.coefficients.reserve(coefficient_values.size());
            for (const auto& coeff : coefficient_values) {
                step.coefficients.push_back(parse_coefficient(coeff));
            }
        }

        scheme.steps.push_back(std::move(step));
    }

    validate_scheme(scheme);
    return scheme;
}

}  // namespace ttwv::cpu
