// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "assets.hpp"
#include "asteroid_field.hpp"
#include "gl_api.hpp"
#include <array>
#include <cstdint>
#include <vector>

namespace spacevoyages {
// All GL calls, including destruction, belong to the transient scene's thread.
// initialize borrows the assets/layout only for that call. draw changes program,
// VAO, texture and opaque depth state; the caller restores its following pass.
class AsteroidRenderer final {
public:
    using Matrix=std::array<float,16>;
    struct Stats {
        unsigned draws{},instances{},culled_batches{};
        std::uint64_t triangles{};
    };
    AsteroidRenderer()=default;
    ~AsteroidRenderer(){close();}
    AsteroidRenderer(const AsteroidRenderer&)=delete;
    AsteroidRenderer& operator=(const AsteroidRenderer&)=delete;
    AsteroidRenderer(AsteroidRenderer&&)=delete;
    AsteroidRenderer& operator=(AsteroidRenderer&&)=delete;
    void initialize(GlApi::GetProc,const SceneAssets&,const AsteroidLayout&);
    // Time is the same paused/interpolated simulation time as the ship/camera.
    // Geometry/instance buffers are immutable. An optional alive mask updates
    // one preallocated RGBA32F texture only when revision changes. Null means
    // all alive. Optional death times defer hiding to the interpolated hit.
    // Facade/device errors may propagate to the scene owner's fallback guard.
    void draw(const Matrix& view_projection,float presentation_seconds,
        const std::uint8_t* alive=nullptr,std::size_t count=0,std::uint64_t revision=0,
        const double* destroyed_seconds=nullptr);
    void close()noexcept;
    bool initialized()const noexcept{return program_!=0;}
    const Stats& last_stats()const noexcept{return stats_;}
private:
    struct Geometry {
        unsigned vbo{},ebo{},texture{};
        int count{};
        AlphaMode alpha=AlphaMode::Opaque;
        float alpha_cutoff=.5f;
        unsigned wrap_s=33071,wrap_t=33071;
        bool atlas{};
    };
    struct Part {unsigned vao{};std::size_t geometry{};};
    struct Batch {
        unsigned instance_vbo{};
        int instances{};
        Vec3 center{};
        float radius{};
        std::vector<Part> parts;
    };
    GlApi gl_{};
    unsigned program_{};
    int vp_{},time_{},alpha_mode_{},alpha_cutoff_{},atlas_mode_{},wrap_s_{},wrap_t_{};
    std::vector<Geometry> geometry_;
    std::vector<Batch> batches_;
    std::vector<unsigned> textures_;
    unsigned alive_texture_{};
    std::size_t asteroid_count_{};
    int mask_height_{};
    std::vector<float> mask_pixels_;
    std::uint64_t mask_revision_{};
    bool custom_mask_{},timed_mask_{};
    Stats stats_{};
    unsigned shader(unsigned,const char*);
    void draw_batch(const Batch&);
};
}
