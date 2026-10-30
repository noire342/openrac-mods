// SPDX-License-Identifier: GPL-3.0-or-later
#include <cstddef>
#include <memory>
#include <stdexcept>
#include <string>

#include "runtime.hpp"

namespace {
cinematic::SteadyClock clock_source;
std::unique_ptr<cinematic::Runtime> session;

void process(std::uint32_t state, std::uint32_t report, std::int32_t size) {
    session->process(state, report, size);
}

void pad(void* context, int port, std::uint8_t* report, int size) {
    static_cast<cinematic::Runtime*>(context)->pad(port, report, size);
}
}  // namespace

extern "C" {
OPENRAC_MOD_DEFINE("cinematic-journeys", "0.1.1")

OPENRAC_MOD_EXPORT int openrac_mod_load(const openrac_api* api, openrac_mod* mod) {
    session.reset();
    if (!api || api->size < offsetof(openrac_api, loaded_level) + sizeof(api->loaded_level)) {
        return 1;
    }
    try {
        if (!api->fail || !api->hook || !api->on_pad) {
            throw std::runtime_error("Cinematic Journeys: missing hook/pad services");
        }
        session = std::make_unique<cinematic::Runtime>(*api, mod, clock_source);
        session->load();
        void* original = nullptr;
        const auto result = api->hook(
            mod,
            "rac1.pad.process_report",
            "void(u32,u32,i32)",
            reinterpret_cast<void*>(process),
            &original
        );
        if (result != 0 || !original) {
            throw std::runtime_error(
                "Cinematic Journeys: Pad_ProcessReport void(u32,u32,i32) hook rejected (code "
                + std::to_string(result) + ")"
            );
        }
        session->set_original(original);
        if (api->on_pad(mod, pad, session.get()) != 0) {
            throw std::runtime_error("Cinematic Journeys: pad callback registration rejected");
        }
        return 0;
    } catch (const std::exception& e) {
        if (api->fail) {
            api->fail(mod, e.what());
        }
        session.reset();
        return 1;
    }
}

OPENRAC_MOD_EXPORT void openrac_mod_unload() {
    session.reset();
}
}
