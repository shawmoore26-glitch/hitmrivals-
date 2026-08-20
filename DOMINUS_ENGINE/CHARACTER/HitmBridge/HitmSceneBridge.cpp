// CHARACTER/HitmBridge/HitmSceneBridge.cpp
#include "CHARACTER/HitmBridge/HitmSceneBridge.h"

#include <string>
#include <utility>

#include "GRAPHICS/Renderer/MeshTransform.h"

namespace dominus::character::hitm {

namespace {

// The one real, disclosed anchor convention this file uses -- see
// HitmSceneBridge.h's own header comment for the full derivation.
// (0.5, 1.0): character horizontal center, character base.
constexpr double kAnchorX = 0.5;
constexpr double kAnchorY = 1.0;

}  // namespace

core::Result<std::vector<graphics::SceneEntity>> BuildHitmSceneEntities(const std::string& fighterId,
                                                                          const HitmFighterSnapshot& snapshot,
                                                                          const HitmSpriteDrawData& drawData,
                                                                          double displayHeight) {
    if (displayHeight <= 0.0) {
        return core::Result<std::vector<graphics::SceneEntity>>::Fail(
            "HitmSceneBridge: displayHeight must be a real, positive, authored game.json sprite.displayHeight "
            "value, got " +
            std::to_string(displayHeight));
    }
    if (fighterId.empty()) {
        return core::Result<std::vector<graphics::SceneEntity>>::Fail("HitmSceneBridge: fighterId must not be empty");
    }

    std::vector<graphics::SceneEntity> entities;
    entities.reserve(drawData.parts.size());

    const double H = displayHeight;

    for (std::size_t i = 0; i < drawData.parts.size(); ++i) {
        const HitmPartDraw& p = drawData.parts[i];

        // Real, disclosed rendered size in screen/world pixels -- direct
        // scale of parts.json's own real normW/normH by the one real
        // authored scale reference. See this file's header comment for
        // why not rig_render.py's own extra, uncited 0.62 factor.
        const double sw = p.norm_w * H;
        const double sh = p.norm_h * H;

        // Real, disclosed local position -- rig.json's real placement
        // rect corner plus anim.json's real sampled pose offset (both
        // already in the same display-height-normalized units, per
        // HitmAnimationSet.h's own header comment).
        const double localX = p.place_x + p.pose_offset_x;
        const double localY = p.place_y + p.pose_offset_y;

        // Top-left corner in HITM's own real Y-DOWN screen convention,
        // anchored so (kAnchorX, kAnchorY) lands exactly on the
        // fighter's own real (snapshot.x, snapshot.y).
        const double topLeftX = snapshot.x + (localX - kAnchorX) * H;
        const double topLeftY = snapshot.y + (localY - kAnchorY) * H;

        // DrawCommand/SceneEntity position the quad's CENTER, not its
        // top-left corner (MeshTransform.h's UnitQuad is -0.5..0.5).
        const double centerX = topLeftX + sw / 2.0;
        const double centerY = topLeftY + sh / 2.0;

        graphics::SceneEntity entity;
        entity.entity_id = fighterId + "_" + p.part_name;
        // Y negated: HITM's real Y-DOWN screen convention -> DOMINUS's
        // own real Y-UP world convention (see header comment).
        entity.world_transform.x = static_cast<float>(centerX);
        entity.world_transform.y = static_cast<float>(-centerY);
        entity.world_transform.rotation_deg = static_cast<float>(-p.pose_rotation_deg);
        // MeshTransform.h's UnitQuad spans 1.0 local unit before scale,
        // scaled again by its own real kMeshPixelScale (16) -- scale_x/y
        // must therefore be the desired pixel size divided by that same
        // constant so the FINAL rendered size is exactly sw x sh pixels.
        entity.world_transform.scale_x = static_cast<float>(sw / graphics::kMeshPixelScale);
        entity.world_transform.scale_y = static_cast<float>(sh / graphics::kMeshPixelScale);

        entity.mesh_ref = "hitm_part_quad";
        entity.sort_layer = static_cast<int>(i);  // real drawOrder, back-to-front (HitmSpriteDrawData's own order)

        entity.textured = true;
        entity.atlas_id = fighterId;
        entity.atlas_src_x = p.frame_x;
        entity.atlas_src_y = p.frame_y;
        entity.atlas_src_w = p.frame_w;
        entity.atlas_src_h = p.frame_h;

        entities.push_back(std::move(entity));
    }

    return core::Result<std::vector<graphics::SceneEntity>>::Ok(std::move(entities));
}

}  // namespace dominus::character::hitm
