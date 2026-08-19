// GRAPHICS/Renderer/FrameCompiler.h
// Scene + Camera -> Frame. Pure, deterministic, hash-verified.
// Deterministic ORDER is a real design decision, not an accident of
// std::vector iteration: commands are sorted by (sort_layer,
// entity_id), a fixed tie-break, so two scenes with the same entities
// added in different orders still produce byte-identical frames.
#pragma once

#include <algorithm>
#include <sstream>

#include "GRAPHICS/Renderer/Camera.h"
#include "GRAPHICS/Renderer/Frame.h"
#include "GRAPHICS/Renderer/Scene.h"
#include "REGISTRY/Hash/Sha256.h"

namespace dominus::graphics {

class FrameCompiler {
public:
    // `viewport` is optional (defaults to {0,0}, "unspecified") --
    // purely descriptive metadata carried in the resulting Frame, see
    // Viewport's own comment in Frame.h for why it does not bind or
    // change any computed screen_transform.
    static Frame Compile(const Scene& scene, const Camera& camera, Viewport viewport = Viewport{}) {
        Frame frame;
        frame.camera = camera;
        frame.viewport = viewport;
        for (const auto& entity : scene.entities) {
            DrawCommand cmd;
            cmd.entity_id = entity.entity_id;
            cmd.screen_transform = ToCameraSpace(camera, entity.world_transform);
            cmd.material_ref = entity.material_ref;
            cmd.mesh_ref = entity.mesh_ref;
            cmd.sort_layer = entity.sort_layer;
            cmd.material_wear_state = entity.material_wear_state;
            cmd.material_resolved = entity.material_resolved;
            cmd.material_r = entity.material_r;
            cmd.material_g = entity.material_g;
            cmd.material_b = entity.material_b;
            frame.commands.push_back(std::move(cmd));
        }

        std::sort(frame.commands.begin(), frame.commands.end(), [](const DrawCommand& a, const DrawCommand& b) {
            if (a.sort_layer != b.sort_layer) return a.sort_layer < b.sort_layer;
            return a.entity_id < b.entity_id;
        });

        frame.frame_hash = registry::Sha256::Hash(Serialize(frame));
        return frame;
    }

    static std::string Serialize(const Frame& frame) {
        std::ostringstream out;
        out << "[camera;x=" << frame.camera.x << ";y=" << frame.camera.y << ";zoom=" << frame.camera.zoom
            << ";rotation=" << frame.camera.rotation_deg << "]";
        out << "[viewport;w=" << frame.viewport.width << ";h=" << frame.viewport.height << "]";
        for (const auto& cmd : frame.commands) {
            const auto& t = cmd.screen_transform;
            out << "[entity=" << cmd.entity_id << ";x=" << t.x << ";y=" << t.y << ";rotation=" << t.rotation_deg
                << ";scale_x=" << t.scale_x << ";scale_y=" << t.scale_y << ";material=" << cmd.material_ref
                << ";mesh=" << cmd.mesh_ref << ";layer=" << cmd.sort_layer << ";wear=" << cmd.material_wear_state
                << ";resolved=" << cmd.material_resolved << ";r=" << static_cast<int>(cmd.material_r)
                << ";g=" << static_cast<int>(cmd.material_g) << ";b=" << static_cast<int>(cmd.material_b) << "]";
        }
        return out.str();
    }
};

}  // namespace dominus::graphics
