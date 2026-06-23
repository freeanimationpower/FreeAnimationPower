#include "engine/brush/brush_engine.hpp"
#include <fstream>
#include <sstream>
#include <charconv>
#include <cstdio>
#include <cstdlib>

namespace fap {

BrushPreset createRoundSoftPreset(float size);
BrushPreset createRoundHardPreset(float size);
BrushPreset createFlatPreset(float size, float angle);
BrushPreset createCalligraphyPreset(float size, float angle);
BrushPreset createEraserPreset(float size);

namespace json_detail {

std::string escape(std::string_view s) {
    std::string out;
    out.reserve(s.size() + 4);
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:   out += c;
        }
    }
    return out;
}

size_t find_key(std::string_view json, std::string_view key, size_t start = 0) {
    std::string search = "\"" + std::string(key) + "\"";
    return json.find(search, start);
}

size_t skip_to_value(std::string_view json, size_t key_end) {
    size_t p = key_end;
    while (p < json.size() && (json[p] == ' ' || json[p] == '\t' || json[p] == '\n' || json[p] == '\r')) ++p;
    if (p < json.size() && json[p] == ':') {
        ++p;
        while (p < json.size() && (json[p] == ' ' || json[p] == '\t' || json[p] == '\n' || json[p] == '\r')) ++p;
    }
    return p;
}

std::string extract_string(std::string_view json, std::string_view key, size_t start = 0,
                           const std::string& default_val = "") {
    size_t k = find_key(json, key, start);
    if (k == std::string::npos) return default_val;
    size_t p = skip_to_value(json, k + key.size() + 2);
    if (p >= json.size() || json[p] != '"') return default_val;
    size_t end = p + 1;
    while (end < json.size()) {
        if (json[end] == '"') break;
        if (json[end] == '\\' && end + 1 < json.size()) ++end;
        ++end;
    }
    if (end >= json.size()) return default_val;
    std::string result;
    result.reserve(end - p);
    for (size_t i = p + 1; i < end; ++i) {
        if (json[i] == '\\' && i + 1 < end) {
            switch (json[++i]) {
                case '"':  result += '"';  break;
                case '\\': result += '\\'; break;
                case 'n':  result += '\n'; break;
                case 'r':  result += '\r'; break;
                case 't':  result += '\t'; break;
                default:   result += json[i]; break;
            }
        } else {
            result += json[i];
        }
    }
    return result;
}

float extract_float(std::string_view json, std::string_view key, size_t start = 0,
                    float default_val = 0.0f) {
    size_t k = find_key(json, key, start);
    if (k == std::string::npos) return default_val;
    size_t p = skip_to_value(json, k + key.size() + 2);
    if (p >= json.size()) return default_val;

    auto* begin = json.data() + p;
    auto* end_ptr = json.data() + json.size();
    float val = default_val;
    auto [ptr, ec] = std::from_chars(begin, end_ptr, val);
    if (ec == std::errc{}) return val;

    char* endp = nullptr;
    val = std::strtof(begin, &endp);
    if (endp != begin) return val;
    return default_val;
}

int extract_int(std::string_view json, std::string_view key, size_t start = 0,
                int default_val = 0) {
    size_t k = find_key(json, key, start);
    if (k == std::string::npos) return default_val;
    size_t p = skip_to_value(json, k + key.size() + 2);
    if (p >= json.size()) return default_val;

    auto* begin = json.data() + p;
    auto* end_ptr = json.data() + json.size();
    int val = default_val;
    auto [ptr, ec] = std::from_chars(begin, end_ptr, val);
    if (ec == std::errc{}) return val;
    return default_val;
}

bool extract_bool(std::string_view json, std::string_view key, size_t start = 0,
                  bool default_val = false) {
    size_t k = find_key(json, key, start);
    if (k == std::string::npos) return default_val;
    size_t p = skip_to_value(json, k + key.size() + 2);
    if (p >= json.size()) return default_val;
    if (json.substr(p, 4) == "true")  return true;
    if (json.substr(p, 5) == "false") return false;
    return default_val;
}

std::vector<float> extract_float_array(std::string_view json, std::string_view key,
                                       size_t start = 0) {
    size_t k = find_key(json, key, start);
    if (k == std::string::npos) return {};
    size_t p = skip_to_value(json, k + key.size() + 2);
    if (p >= json.size() || json[p] != '[') return {};

    size_t end = json.find(']', p);
    if (end == std::string::npos) return {};

    std::vector<float> result;
    std::string_view arr = json.substr(p + 1, end - p - 1);
    size_t pos = 0;
    while (pos < arr.size()) {
        while (pos < arr.size() && (arr[pos] == ' ' || arr[pos] == '\t' ||
               arr[pos] == '\n' || arr[pos] == '\r' || arr[pos] == ',')) ++pos;
        if (pos >= arr.size()) break;
        auto* begin = arr.data() + pos;
        auto* e = arr.data() + arr.size();
        float val = 0.0f;
        auto [ptr, ec] = std::from_chars(begin, e, val);
        if (ec == std::errc{} && ptr != begin) {
            result.push_back(val);
            pos = ptr - arr.data();
        } else {
            break;
        }
    }
    return result;
}

std::string_view extract_object(std::string_view json, std::string_view key, size_t start = 0) {
    size_t k = find_key(json, key, start);
    if (k == std::string::npos) return {};
    size_t p = skip_to_value(json, k + key.size() + 2);
    if (p >= json.size() || json[p] != '{') return {};

    int depth = 1;
    size_t i = p + 1;
    bool in_string = false;
    while (i < json.size() && depth > 0) {
        char c = json[i];
        if (c == '"' && (i == 0 || json[i - 1] != '\\')) in_string = !in_string;
        if (!in_string) {
            if (c == '{') ++depth;
            else if (c == '}') --depth;
        }
        ++i;
    }
    if (depth == 0) return json.substr(p, i - p);
    return {};
}

} // namespace json_detail

BrushEngine::BrushEngine() {
    presets_.push_back(createRoundSoftPreset(10.0f));
    presets_.push_back(createRoundHardPreset(5.0f));
    presets_.push_back(createFlatPreset(8.0f, 0.0f));
    presets_.push_back(createCalligraphyPreset(12.0f, 45.0f));
    presets_.push_back(createEraserPreset(20.0f));

    current_preset_ = presets_.front();
}

BrushEngine::~BrushEngine() = default;

void BrushEngine::setPreset(const BrushPreset& preset) {
    current_preset_ = preset;
}

void BrushEngine::beginStroke(const Color& color, float size) {
    stroking_ = true;
    stroke_points_.clear();
    current_preset_.color = color;
    current_preset_.size = size;
}

void BrushEngine::addPoint(const StrokePoint& point) {
    if (stroking_) {
        stroke_points_.push_back(point);
    }
}

void BrushEngine::endStroke() {
    stroking_ = false;
}

void BrushEngine::loadPresets(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return;

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();
    file.close();

    // Parse into a temporary vector; only commit on success
    std::vector<BrushPreset> parsed;

    size_t presets_key = content.find("\"presets\"");
    if (presets_key == std::string::npos) return;

    size_t array_start = content.find('[', presets_key);
    if (array_start == std::string::npos) return;

    size_t pos = array_start + 1;
    while (pos < content.size()) {
        while (pos < content.size() && (content[pos] == ' ' || content[pos] == '\t' ||
               content[pos] == '\n' || content[pos] == '\r' || content[pos] == ',')) ++pos;
        if (pos >= content.size() || content[pos] == ']') break;
        if (content[pos] != '{') break;

        int depth = 1;
        size_t obj_start = pos;
        size_t obj_end = pos + 1;
        bool in_string = false;
        while (obj_end < content.size() && depth > 0) {
            char c = content[obj_end];
            if (c == '"' && (obj_end == 0 || content[obj_end - 1] != '\\')) in_string = !in_string;
            if (!in_string) {
                if (c == '{') ++depth;
                else if (c == '}') --depth;
            }
            ++obj_end;
        }

        std::string_view obj(content.data() + obj_start, obj_end - obj_start);

        BrushPreset preset;
        preset.name              = json_detail::extract_string(obj, "name");
        preset.mode              = static_cast<BrushMode>(json_detail::extract_int(obj, "mode"));
        preset.size              = json_detail::extract_float(obj, "size", 0, 10.0f);
        preset.opacity           = json_detail::extract_float(obj, "opacity", 0, 1.0f);
        preset.flow              = json_detail::extract_float(obj, "flow", 0, 0.3f);
        preset.blend_mode        = static_cast<BlendMode>(json_detail::extract_int(obj, "blend_mode"));
        preset.use_paper_texture  = json_detail::extract_bool(obj, "use_paper_texture");
        preset.paper_texture_path = json_detail::extract_string(obj, "paper_texture_path");

        auto color_arr = json_detail::extract_float_array(obj, "color");
        if (color_arr.size() >= 4) {
            preset.color = { color_arr[0], color_arr[1], color_arr[2], color_arr[3] };
        }

        auto tip_obj = json_detail::extract_object(obj, "tip");
        if (!tip_obj.empty()) {
            preset.tip.name      = json_detail::extract_string(tip_obj, "name");
            preset.tip.imagePath = json_detail::extract_string(tip_obj, "imagePath");
            preset.tip.spacing   = json_detail::extract_float(tip_obj, "spacing", 0, 0.15f);
            preset.tip.hardness  = json_detail::extract_float(tip_obj, "hardness", 0, 0.8f);
            preset.tip.angle     = json_detail::extract_float(tip_obj, "angle");
            preset.tip.roundness = json_detail::extract_float(tip_obj, "roundness", 0, 1.0f);
        }

        auto dyn_obj = json_detail::extract_object(obj, "dynamics");
        if (!dyn_obj.empty()) {
            preset.dynamics.pressure_opacity = json_detail::extract_bool(dyn_obj, "pressure_opacity", 0, true);
            preset.dynamics.pressure_size    = json_detail::extract_bool(dyn_obj, "pressure_size", 0, true);
            preset.dynamics.pressure_flow    = json_detail::extract_bool(dyn_obj, "pressure_flow", 0, true);
            preset.dynamics.tilt_angle       = json_detail::extract_bool(dyn_obj, "tilt_angle");
            preset.dynamics.min_size         = json_detail::extract_float(dyn_obj, "min_size", 0, 0.1f);
            preset.dynamics.max_size         = json_detail::extract_float(dyn_obj, "max_size", 0, 2.0f);
            preset.dynamics.min_opacity      = json_detail::extract_float(dyn_obj, "min_opacity", 0, 0.1f);
            preset.dynamics.max_opacity      = json_detail::extract_float(dyn_obj, "max_opacity", 0, 1.0f);
            preset.dynamics.min_flow         = json_detail::extract_float(dyn_obj, "min_flow", 0, 0.05f);
            preset.dynamics.max_flow         = json_detail::extract_float(dyn_obj, "max_flow", 0, 0.3f);
            preset.dynamics.smoothing        = json_detail::extract_float(dyn_obj, "smoothing", 0, 0.5f);
        }

        parsed.push_back(std::move(preset));
        pos = obj_end;
    }

    if (!parsed.empty()) {
        presets_ = std::move(parsed);
        current_preset_ = presets_.front();
    }
}

void BrushEngine::savePresets(const std::string& path) {
    std::ofstream file(path);
    if (!file.is_open()) return;

    auto wf = [&](const std::string& indent, const std::string& key, float val, bool last = false) {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%.6g", val);
        file << indent << "\"" << key << "\": " << buf << (last ? "\n" : ",\n");
    };
    auto wi = [&](const std::string& indent, const std::string& key, int val, bool last = false) {
        file << indent << "\"" << key << "\": " << val << (last ? "\n" : ",\n");
    };
    auto wb = [&](const std::string& indent, const std::string& key, bool val, bool last = false) {
        file << indent << "\"" << key << "\": " << (val ? "true" : "false") << (last ? "\n" : ",\n");
    };
    auto ws = [&](const std::string& indent, const std::string& key, const std::string& val, bool last = false) {
        file << indent << "\"" << key << "\": \"" << json_detail::escape(val) << "\"" << (last ? "\n" : ",\n");
    };
    auto wa = [&](const std::string& indent, const std::string& key,
                  const std::vector<float>& vals, bool last = false) {
        file << indent << "\"" << key << "\": [";
        for (size_t i = 0; i < vals.size(); ++i) {
            char buf[64];
            std::snprintf(buf, sizeof(buf), "%.6g", vals[i]);
            file << buf;
            if (i + 1 < vals.size()) file << ", ";
        }
        file << "]" << (last ? "\n" : ",\n");
    };

    file << "{\n";
    file << "  \"presets\": [\n";

    for (size_t pi = 0; pi < presets_.size(); ++pi) {
        const auto& p = presets_[pi];
        file << "    {\n";

        ws("      ", "name",               p.name);
        wi("      ", "mode",               static_cast<int>(p.mode));
        wf("      ", "size",               p.size);
        wf("      ", "opacity",            p.opacity);
        wa("      ", "color",              { p.color.r, p.color.g, p.color.b, p.color.a });
        wi("      ", "blend_mode",         static_cast<int>(p.blend_mode));
        wf("      ", "flow",               p.flow);
        wb("      ", "use_paper_texture",  p.use_paper_texture);
        ws("      ", "paper_texture_path", p.paper_texture_path);

        file << "      \"tip\": {\n";
        ws("        ", "name",      p.tip.name);
        ws("        ", "imagePath", p.tip.imagePath);
        wf("        ", "spacing",   p.tip.spacing);
        wf("        ", "hardness",  p.tip.hardness);
        wf("        ", "angle",     p.tip.angle);
        wf("        ", "roundness", p.tip.roundness, true);
        file << "      },\n";

        file << "      \"dynamics\": {\n";
        wb("        ", "pressure_opacity", p.dynamics.pressure_opacity);
        wb("        ", "pressure_size",    p.dynamics.pressure_size);
        wb("        ", "pressure_flow",    p.dynamics.pressure_flow);
        wb("        ", "tilt_angle",       p.dynamics.tilt_angle);
        wf("        ", "min_size",         p.dynamics.min_size);
        wf("        ", "max_size",         p.dynamics.max_size);
        wf("        ", "min_opacity",      p.dynamics.min_opacity);
        wf("        ", "max_opacity",      p.dynamics.max_opacity);
        wf("        ", "min_flow",         p.dynamics.min_flow);
        wf("        ", "max_flow",         p.dynamics.max_flow);
        wf("        ", "smoothing",        p.dynamics.smoothing, true);
        file << "      }\n";

        file << "    }";
        if (pi + 1 < presets_.size()) file << ",";
        file << "\n";
    }

    file << "  ]\n";
    file << "}\n";

    file.close();
}

} // namespace fap
