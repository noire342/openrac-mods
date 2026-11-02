// SPDX-License-Identifier: GPL-3.0-or-later
// Isolated native QA only. Never include this plugin in a distributable mod.
// Without an explicit fixture variable, every game-memory access is read-only.
#include <openrac/openrac_mod.h>
#include <array>
#include <cerrno>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <locale>
#include <memory>
#include <stdexcept>
#include <string>

namespace {
constexpr std::uint32_t pal_hero = 0x0013F450;
// Independent reference: work/clank-lab/pal-bindings.json, derived from
// games/rac1/config/overlays/functions.tsv at official commit d015ea1c.
constexpr std::array<std::uint32_t, 19> pal_state_addresses{
    0x00222B80u, 0x0023D688u, 0x0022BE40u, 0x00216D38u, 0x002160E0u, 0x0024D680u, 0x00235E08u,
    0x0024A968u, 0x00231228u, 0x00242760u, 0x00216168u, 0x0024E240u, 0x002407C0u, 0x0022E5E8u,
    0x002306C8u, 0x00217348u, 0x0021EA88u, 0x0021ED38u, 0x002284E0u};

void number(std::ostream &out, float value) {
    if (std::isfinite(value))
        out << value;
    else
        out << "null";
}
void vector(std::ostream &out, const float *values, unsigned count = 3) {
    out << '[';
    for (unsigned i = 0; i < count; ++i) {
        if (i)
            out << ',';
        number(out, values[i]);
    }
    out << ']';
}
std::string environment(const char *name) {
#if defined(_WIN32)
    char *value{};
    std::size_t bytes{};
    if (_dupenv_s(&value, &bytes, name) != 0)
        throw std::runtime_error("Cannot read a PAL QA environment option");
    const std::unique_ptr<char, decltype(&std::free)> owner(value, &std::free);
    return value ? std::string(value) : std::string{};
#else
    const char *value = std::getenv(name);
    return value ? std::string(value) : std::string{};
#endif
}
std::array<float, 3> parse_position(const char *text) {
    std::array<float, 3> values{};
    const char *at = text;
    for (unsigned i = 0; i < values.size(); ++i) {
        char *end{};
        errno = 0;
        values[i] = std::strtof(at, &end);
        if (at == end || errno == ERANGE || !std::isfinite(values[i]))
            throw std::runtime_error("Invalid OPENRAC_CLANK_QA_WARP position");
        if (i < 2) {
            if (*end != ',')
                throw std::runtime_error("OPENRAC_CLANK_QA_WARP must be x,y,z");
            at = end + 1;
        } else if (*end) {
            throw std::runtime_error("Unexpected data after OPENRAC_CLANK_QA_WARP");
        }
    }
    return values;
}
int parse_state(const char *text) {
    char *end{};
    errno = 0;
    const long state = std::strtol(text, &end, 0);
    if (text == end || *end || errno == ERANGE || state < 0 || state > 0x82)
        throw std::runtime_error("OPENRAC_CLANK_QA_STATE must be an integer in 0..0x82");
    return static_cast<int>(state);
}

class Probe final {
  public:
    Probe(const openrac_api &api, openrac_mod *mod) : api_(api), mod_(mod) {
        const auto path = environment("OPENRAC_CLANK_PROBE");
        if (path.empty())
            throw std::runtime_error("Set OPENRAC_CLANK_PROBE to an explicit absolute JSONL path");
        const auto output = std::filesystem::u8path(path);
        if (!output.is_absolute() || std::filesystem::exists(output))
            throw std::runtime_error("PAL probe requires a new absolute output file");
        if (const auto warp = environment("OPENRAC_CLANK_QA_WARP"); !warp.empty()) {
            warp_ = parse_position(warp.c_str());
            warp_enabled_ = true;
        }
        if (const auto back = environment("OPENRAC_CLANK_QA_RETURN"); !back.empty()) {
            if (!warp_enabled_)
                throw std::runtime_error("RETURN requires WARP");
            return_position_ = parse_position(back.c_str());
            return_enabled_ = true;
        }
        if (const auto state = environment("OPENRAC_CLANK_QA_STATE"); !state.empty()) {
            requested_state_ = parse_state(state.c_str());
            state_enabled_ = true;
        }
        // Each run proves either a real terrain interaction or a synthetic state,
        // so the two kinds of evidence cannot accidentally be combined.
        if (warp_enabled_ && state_enabled_)
            throw std::runtime_error("Select only one PAL QA fixture per run: WARP or STATE");
        out_.open(output, std::ios::binary | std::ios::out);
        if (!out_)
            throw std::runtime_error("Cannot open the PAL QA probe output");
        out_.imbue(std::locale::classic());
        out_ << std::setprecision(std::numeric_limits<float>::max_digits10);
    }

    void started(int hit_observer_result) {
        out_ << "{\"event\":\"probe_start\",\"region\":\"pal\",\"fixture\":\""
             << (warp_enabled_    ? "warp"
                 : state_enabled_ ? "state"
                                  : "none")
             << "\",\"hit_observer_result\":" << hit_observer_result << "}\n";
        flush();
    }
    void frame() {
        ++frames_;
        const auto *input = api_.input ? api_.input() : nullptr;
        tick_ = input ? input->tick : frames_;
        openrac_play_status status{};
        status.size = sizeof(status);
        const int status_result = api_.play_status(&status);
        openrac_pose pose{};
        pose.size = sizeof(pose);
        const int pose_result = api_.hero_pose(&pose);
        const auto hero = api_.relocate_data(pal_hero);
        const auto body = read<std::uint32_t>(hero + 0x2080);
        const auto rotor = read<std::uint32_t>(hero + 0x1620);
        const auto glow = read<std::uint32_t>(hero + 0x1624);
        const auto mode = read<std::uint8_t>(hero + 0x20A4);
        const auto body_class = body ? read<std::uint16_t>(body + 0xA6) : -1;
        if (body_class == 87) {
            const auto meter = api_.relocate_data(0x00199C60) + 4 * 0x90;
            const auto charge = read<std::uint32_t>(meter + 0xC);
            const auto definition = charge + 16;
            for (unsigned slot = 0; slot < 30; ++slot) {
                const auto voice = api_.relocate_data(0x0013E650) + 0x70 * (slot + 1);
                if (read<std::uint32_t>(voice + 8) == definition && read<std::uint8_t>(voice + 4))
                    out_ << "{\"event\":\"warning_voice\",\"tick\":" << tick_
                         << ",\"slot\":" << slot
                         << ",\"state\":" << unsigned(read<std::uint8_t>(voice + 4))
                         << ",\"handle\":" << read<std::uint32_t>(voice)
                         << ",\"flags\":" << unsigned(read<std::uint8_t>(voice + 5))
                         << ",\"bank\":" << read<std::uint32_t>(definition + 0x1C)
                         << ",\"sound\":" << read<std::int16_t>(definition + 0x1A)
                         << ",\"volume\":" << read<int>(voice + 0x10) << "}\n";
            }
        }
        if (body_class == 87 && !hud_assets_logged_) {
            hud_assets_logged_ = true;
            using Icon = int (*)(int, int);
            const auto icon = reinterpret_cast<Icon>(api_.function("func_00200198"));
            const auto multiply =
                reinterpret_cast<float (*)(float)>(api_.function("func_001F9878"));
            const auto arena = api_.relocate_data(0x0019A4E8);
            const auto definitions = read<std::uint32_t>(arena + 0x1C);
            out_ << "{\"event\":\"hud_assets\",\"arena\":" << arena
                 << ",\"level_arena\":" << api_.relocate_level_data(0, 0x0017E5D8)
                 << ",\"arena_2c\":" << read<int>(arena + 0x2C)
                 << ",\"frame_scale_sq\":" << read<float>(api_.relocate_data(0x0015EE64))
                 << ",\"meter_width_address\":" << api_.relocate_level_data(0, 0x0015F840)
                 << ",\"meter_height_address\":" << api_.relocate_level_data(0, 0x0015F844)
                 << ",\"meter_width\":" << read<int>(api_.relocate_level_data(0, 0x0015F840))
                 << ",\"meter_height\":" << read<int>(api_.relocate_level_data(0, 0x0015F844))
                 << ",\"meter_resident_width\":" << read<int>(api_.relocate_data(0x0015F840))
                 << ",\"meter_resident_height\":" << read<int>(api_.relocate_data(0x0015F844))
                 << ",\"native_scaled_075\":" << (multiply ? multiply(0.75f) : -1)
                 << ",\"native_counter_address\":" << api_.relocate_level_data(0, 0x0015F4F8)
                 << ",\"icons\":[";
            for (int k = 0; k < 8; ++k) {
                if (k)
                    out_ << ',';
                out_ << (icon ? icon(0x7553, k) : -1);
            }
            out_ << "],\"definitions\":[";
            bool comma = false;
            for (unsigned i = 0; i < 2048; ++i) {
                const auto at = definitions + i * 8;
                const auto id = read<std::uint16_t>(at);
                if (id == 0xFFFF)
                    break;
                if (id != 0x7553 && id != 0x753F && id != 0x7FFE && id != 0x7FFD)
                    continue;
                if (comma)
                    out_ << ',';
                comma = true;
                out_ << "{\"id\":" << id << ",\"count\":" << read<std::uint16_t>(at + 2)
                     << ",\"start\":" << read<std::uint16_t>(at + 4) << '}';
            }
            out_ << "]}\n";
        }
        const std::array<float, 3> capsule{read<float>(hero + 0x220), read<float>(hero + 0x224),
                                           read<float>(hero + 0x234)};
        const std::array<float, 3> capsule_target{
            read<float>(hero + 0x228), read<float>(hero + 0x22C), read<float>(hero + 0x230)};
        out_ << "{\"event\":\"frame\",\"tick\":" << tick_ << ",\"probe_frame\":" << frames_
             << ",\"status_result\":" << status_result << ",\"phase\":" << status.phase
             << ",\"level\":" << status.level << ",\"loaded_level\":" << api_.loaded_level()
             << ",\"loads\":" << status.loads << ",\"mode\":" << static_cast<unsigned>(mode)
             << ",\"body\":" << body << ",\"body_class\":" << body_class
             << ",\"pose_result\":" << pose_result << ",\"position\":";
        vector(out_, pose.position);
        out_ << ",\"velocity\":";
        vector(out_, pose.velocity);
        out_ << ",\"rotation\":";
        vector(out_, pose.rotation);
        out_ << ",\"state\":" << pose.state
             << ",\"native_state\":" << read<std::int32_t>(hero + 0x2084)
             << ",\"control_mode\":" << read<std::int32_t>(hero + 0x208C)
             << ",\"health\":" << pose.health << ",\"seq\":" << pose.seq
             << ",\"frame\":" << pose.frame << ",\"next_frame\":" << pose.next_frame
             << ",\"anim_time\":";
        number(out_, pose.anim_time);
        out_ << ",\"anim_speed\":";
        number(out_, pose.anim_speed);
        out_ << ",\"flags\":" << pose.flags << ",\"capsule\":";
        vector(out_, capsule.data());
        out_ << ",\"capsule_target\":";
        vector(out_, capsule_target.data());
        out_ << ",\"ground_type\":" << read<std::int16_t>(hero + 0x12E0)
             << ",\"selector_4\":" << static_cast<unsigned>(read<std::uint8_t>(hero + 0x12E3))
             << ",\"base_condition\":" << static_cast<unsigned>(read<std::uint8_t>(hero + 0x12E4))
             << ",\"selector_13\":" << static_cast<unsigned>(read<std::uint8_t>(hero + 0x12EC))
             << ",\"water_surface_z\":";
        number(out_, read<float>(hero + 0x2F0));
        out_ << ",\"air\":" << read<int>(hero + 0x22A0)
             << ",\"native_counter\":" << read<int>(api_.relocate_level_data(0, 0x0015F4F8))
             << ",\"command\":" << read<int>(api_.relocate_data(0x00141710))
             << ",\"pad_mode\":" << read<int>(api_.relocate_data(0x0013CA40) + 0x1CC)
             << ",\"hud\":[";
        for (unsigned slot : {3u, 4u}) {
            if (slot == 4)
                out_ << ',';
            const auto entry = api_.relocate_data(0x00199C60) + 0x90 * slot;
            out_ << "{\"slot\":" << slot << ",\"texture\":" << read<int>(entry)
                 << ",\"id\":" << read<int>(entry + 0x64)
                 << ",\"pending_texture\":" << read<int>(entry + 0x20)
                 << ",\"init\":" << read<std::uint32_t>(entry + 0x10)
                 << ",\"update\":" << read<std::uint32_t>(entry + 0x14)
                 << ",\"draw\":" << read<std::uint32_t>(entry + 0x18)
                 << ",\"fade\":" << static_cast<unsigned>(read<std::uint8_t>(entry + 0x70))
                 << ",\"value\":" << read<int>(entry + 0x74)
                 << ",\"selection\":" << read<int>(entry + 0x74) << '}';
        }
        out_ << ']';
        out_ << ",\"rotor\":{\"pointer\":" << rotor
             << ",\"class\":" << (rotor ? static_cast<int>(read<std::uint16_t>(rotor + 0xA6)) : -1)
             << ",\"state\":" << static_cast<unsigned>(read<std::uint8_t>(rotor + 0x20))
             << ",\"flags\":" << read<std::uint16_t>(rotor + 0x34)
             << ",\"cur_seq\":" << static_cast<unsigned>(read<std::uint8_t>(rotor + 0x52))
             << ",\"target_seq\":" << static_cast<unsigned>(read<std::uint8_t>(rotor + 0x53))
             << ",\"frame\":" << static_cast<unsigned>(read<std::uint8_t>(rotor + 0x50))
             << ",\"next_frame\":" << static_cast<unsigned>(read<std::uint8_t>(rotor + 0x51))
             << ",\"anim_time\":";
        number(out_, read<float>(rotor + 0x54));
        out_ << ",\"anim_speed\":";
        number(out_, read<float>(rotor + 0x58));
        out_ << "},\"glow\":{\"pointer\":" << glow
             << ",\"class\":" << (glow ? static_cast<int>(read<std::uint16_t>(glow + 0xA6)) : -1)
             << '}';
        out_ << "}\n";
        flush();

        const bool clank = status_result == 0 && pose_result == 0 &&
                           status.phase == OPENRAC_PLAY_PLAYING && mode == 1 && body_class == 87;
        if (!clank || status.loads != fixture_epoch_) {
            fixture_epoch_ = status.loads;
            clank_frames_ = 0;
        }
        if (clank)
            ++clank_frames_;
        if (clank && !fixture_done_ && clank_frames_ == 60 && (warp_enabled_ || state_enabled_)) {
            fixture_done_ = true;
            fixture(pose);
        }
        if (clank && return_enabled_ && !return_done_ && clank_frames_ == 240) {
            return_done_ = true;
            warp_ = return_position_;
            fixture(pose);
        }
    }
    void hit(const openrac_hit &event) {
        out_ << "{\"event\":\"hit\",\"tick\":" << tick_ << ",\"probe_frame\":" << frames_
             << ",\"victim\":" << event.victim << ",\"attacker\":" << event.attacker
             << ",\"mask\":" << event.mask << ",\"damage\":";
        number(out_, event.damage);
        out_ << ",\"position\":";
        vector(out_, event.position);
        out_ << ",\"velocity\":";
        vector(out_, event.velocity);
        out_ << "}\n";
        flush();
    }
    void failure(const char *message) noexcept { api_.log(mod_, OPENRAC_LOG_ERROR, message); }

  private:
    bool hud_assets_logged_{};
    template <class T> T read(std::uint32_t address) const {
        T value{};
        if (address >= 0x10000 && address <= 0x02000000 - sizeof(T))
            std::memcpy(&value, api_.memory + address, sizeof(value));
        return value;
    }
    void flush() {
        out_.flush();
        if (!out_)
            throw std::runtime_error("Cannot persist the PAL QA probe trace");
    }
    void fixture(const openrac_pose &pose) {
        const int level = api_.loaded_level();
        if (level < 0 || level >= static_cast<int>(pal_state_addresses.size()))
            throw std::runtime_error("PAL QA fixture has no active native level");
        if (state_enabled_) {
            using State = int (*)(int, int);
            const auto enter =
                reinterpret_cast<State>(api_.function_at(pal_state_addresses[level]));
            if (!enter)
                throw std::runtime_error("PAL QA state function is unavailable");
            out_ << "{\"event\":\"fixture_state_begin\",\"tick\":" << tick_
                 << ",\"probe_frame\":" << frames_ << ",\"requested_state\":" << requested_state_
                 << "}\n";
            flush();
            const int result = enter(requested_state_, 1);
            out_ << "{\"event\":\"fixture_state_end\",\"tick\":" << tick_
                 << ",\"probe_frame\":" << frames_ << ",\"result\":" << result << "}\n";
            flush();
            return;
        }
        using Reset = void (*)(std::uint32_t, std::uint32_t, int, int);
        const auto reset = reinterpret_cast<Reset>(api_.function("func_L00_00217718"));
        if (!reset)
            throw std::runtime_error("PAL QA reset function is unavailable");
        openrac_gaddr saved{};
        const auto address = api_.stack_push(32, &saved);
        if (!address)
            throw std::runtime_error("PAL QA cannot reserve native argument vectors");
        struct Stack final {
            const openrac_api &api;
            openrac_gaddr saved;
            ~Stack() { api.stack_pop(saved); }
        } stack{api_, saved};
        const std::array<float, 4> position{warp_[0], warp_[1], warp_[2], 0};
        const std::array<float, 4> rotation{pose.rotation[0], pose.rotation[1], pose.rotation[2],
                                            0};
        std::memset(api_.memory + address, 0, 32);
        std::memcpy(api_.memory + address, position.data(), 16);
        std::memcpy(api_.memory + address + 16, rotation.data(), 16);
        out_ << "{\"event\":\"fixture_warp_begin\",\"tick\":" << tick_
             << ",\"probe_frame\":" << frames_ << ",\"from\":";
        vector(out_, pose.position);
        out_ << ",\"target\":";
        vector(out_, warp_.data());
        out_ << ",\"reset_state\":-1}\n";
        flush();
        // -1 preserves the current native Clank state. This is an explicitly
        // enabled test fixture, not a state correction or part of the mod.
        reset(address, address + 16, -1, 1);
        out_ << "{\"event\":\"fixture_warp_end\",\"tick\":" << tick_
             << ",\"probe_frame\":" << frames_ << "}\n";
        flush();
    }
    const openrac_api &api_;
    openrac_mod *mod_;
    std::ofstream out_;
    std::array<float, 3> warp_{};
    std::array<float, 3> return_position_{};
    bool return_enabled_{}, return_done_{};
    std::uint32_t frames_{}, tick_{}, clank_frames_{}, fixture_epoch_{};
    int requested_state_{};
    bool warp_enabled_{}, state_enabled_{}, fixture_done_{};
};
std::unique_ptr<Probe> probe;

void frame(void *) noexcept {
    if (!probe)
        return;
    try {
        probe->frame();
    } catch (const std::exception &error) {
        probe->failure(error.what());
    }
}
int hit(void *, const openrac_hit *event) noexcept {
    if (probe && event && event->size >= sizeof(openrac_hit)) {
        try {
            probe->hit(*event);
        } catch (const std::exception &error) {
            probe->failure(error.what());
        }
    }
    return 0; // Observe only: never take a hit away from the game.
}
} // namespace

extern "C" {
OPENRAC_MOD_DEFINE("clank-pal-qa", "0.0.1")
OPENRAC_MOD_EXPORT int openrac_mod_load(const openrac_api *api, openrac_mod *mod) {
    if (!api || api->size < offsetof(openrac_api, hero_held) + sizeof(api->hero_held))
        return 1;
    try {
        if (api->api_major != 1 || api->api_minor < 9 || !api->game ||
            std::strcmp(api->game, "rac1") || !api->version || std::strcmp(api->version, "pal") ||
            !api->memory || !api->relocate_data || !api->hero_pose || !api->play_status ||
            !api->loaded_level || !api->function || !api->function_at || !api->stack_push ||
            !api->stack_pop || !api->on_frame || !api->log)
            throw std::runtime_error("The isolated Clank QA probe requires PAL API 1.9 services");
        probe = std::make_unique<Probe>(*api, mod);
        if (api->on_frame(mod, frame, nullptr) != 0)
            throw std::runtime_error("Cannot register the PAL QA frame observer");
        const int hits =
            api->on_moby_hit ? api->on_moby_hit(mod, hit, nullptr) : OPENRAC_HOOK_UNKNOWN;
        probe->started(hits);
        return 0;
    } catch (const std::exception &error) {
        if (api->fail)
            api->fail(mod, error.what());
        probe.reset();
        return 1;
    }
}
OPENRAC_MOD_EXPORT void openrac_mod_unload() { probe.reset(); }
}
