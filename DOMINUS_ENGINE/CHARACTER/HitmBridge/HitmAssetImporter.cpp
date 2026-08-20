// CHARACTER/HitmBridge/HitmAssetImporter.cpp
#include "CHARACTER/HitmBridge/HitmAssetImporter.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <fstream>

namespace dominus::character::hitm {

using core::Result;
using core::json::Value;

namespace {

uint32_t ReadBigEndianU32(const unsigned char* p) {
    return (static_cast<uint32_t>(p[0]) << 24) | (static_cast<uint32_t>(p[1]) << 16) | (static_cast<uint32_t>(p[2]) << 8) |
           static_cast<uint32_t>(p[3]);
}

}  // namespace

Result<std::pair<int32_t, int32_t>> HitmAssetImporter::ReadPngDimensions(const std::filesystem::path& pngPath) {
    if (!std::filesystem::exists(pngPath) || !std::filesystem::is_regular_file(pngPath)) {
        return Result<std::pair<int32_t, int32_t>>::Fail("Atlas file does not exist: " + pngPath.string());
    }

    std::ifstream in(pngPath, std::ios::binary);
    if (!in) {
        return Result<std::pair<int32_t, int32_t>>::Fail("Cannot open atlas file: " + pngPath.string());
    }

    // 8-byte PNG signature + 4-byte chunk length + 4-byte chunk type
    // ("IHDR") + 4-byte width + 4-byte height, all big-endian -- every
    // valid PNG has this as its unconditional first 24 bytes per the PNG
    // standard.
    std::array<unsigned char, 24> header{};
    in.read(reinterpret_cast<char*>(header.data()), header.size());
    if (!in || static_cast<size_t>(in.gcount()) != header.size()) {
        return Result<std::pair<int32_t, int32_t>>::Fail("Atlas file is too short to be a valid PNG: " + pngPath.string());
    }

    static constexpr std::array<unsigned char, 8> kPngSignature = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
    if (!std::equal(kPngSignature.begin(), kPngSignature.end(), header.begin())) {
        return Result<std::pair<int32_t, int32_t>>::Fail("Atlas file does not start with the real PNG signature: " +
                                                            pngPath.string());
    }

    if (header[12] != 'I' || header[13] != 'H' || header[14] != 'D' || header[15] != 'R') {
        return Result<std::pair<int32_t, int32_t>>::Fail(
            "Atlas file's first chunk is not IHDR (not a valid PNG, or a corrupted/truncated one): " + pngPath.string());
    }

    int32_t width = static_cast<int32_t>(ReadBigEndianU32(&header[16]));
    int32_t height = static_cast<int32_t>(ReadBigEndianU32(&header[20]));
    if (width <= 0 || height <= 0) {
        return Result<std::pair<int32_t, int32_t>>::Fail("Atlas file declares non-positive dimensions: " + pngPath.string());
    }

    return Result<std::pair<int32_t, int32_t>>::Ok(std::make_pair(width, height));
}

Result<HitmAssetBundle> HitmAssetImporter::Import(const HitmIdentityRecord& record,
                                                     const std::filesystem::path& hitmEngineRoot) {
    const std::string& fighterId = record.fighter_id;

    try {
        const Value* sprite = record.identity.Get("sprite");
        if (!sprite || !sprite->IsObject()) {
            throw std::runtime_error("identity.json has no 'sprite' object for fighter '" + fighterId + "'");
        }

        auto reqStr = [&](const char* key) -> std::string {
            const Value* v = sprite->Get(key);
            if (!v || !v->IsString()) {
                throw std::runtime_error("identity.json 'sprite' is missing required string field '" + std::string(key) + "'");
            }
            return v->AsString();
        };

        std::string atlasName = reqStr("atlas");
        std::string rigRelPath = reqStr("rig");   // real key name; real value is parts.json's path
        std::string animRelPath = reqStr("anim");

        std::filesystem::path rigPath = hitmEngineRoot / rigRelPath;
        std::filesystem::path animPath = hitmEngineRoot / animRelPath;
        std::filesystem::path characterDir = rigPath.parent_path();

        // Real, evidenced convention cross-check: identity.json's own
        // declared paths must actually point at
        // data/characters/<fighter_id>/{parts,anim}.json -- both files
        // living in the same directory, that directory named after this
        // fighter.
        if (characterDir.filename().string() != fighterId) {
            throw std::runtime_error("identity.json 'sprite.rig' path '" + rigRelPath +
                                      "' does not live under a directory named '" + fighterId + "'");
        }
        if (rigPath.filename().string() != "parts.json") {
            throw std::runtime_error("identity.json 'sprite.rig' path '" + rigRelPath + "' does not name parts.json");
        }
        if (animPath.parent_path() != characterDir || animPath.filename().string() != "anim.json") {
            throw std::runtime_error("identity.json 'sprite.anim' path '" + animRelPath +
                                      "' is not anim.json in the same directory as 'sprite.rig'");
        }

        HitmAssetBundle bundle;
        bundle.fighter_id = fighterId;

        auto partsResult = HitmPartsRig::Import(characterDir);
        if (!partsResult.ok) throw std::runtime_error(partsResult.error);
        bundle.parts = std::move(*partsResult.value);

        // Real cross-file consistency: parts.json's own `atlas` field
        // (already validated by HitmPartsRig::Import against the
        // "<dir>_atlas" convention) must also agree with identity.json's
        // independently-authored `sprite.atlas` for the same fighter --
        // two real, separately-generated files describing the same
        // atlas name.
        if (bundle.parts.Atlas() != atlasName) {
            throw std::runtime_error("identity.json 'sprite.atlas' ('" + atlasName + "') disagrees with parts.json 'atlas' ('" +
                                      bundle.parts.Atlas() + "')");
        }

        auto animResult = HitmAnimationSet::Import(characterDir);
        if (!animResult.ok) throw std::runtime_error(animResult.error);
        bundle.animations = std::move(*animResult.value);

        auto rigResult = HitmRigPlacement::Import(characterDir, &bundle.parts);
        if (!rigResult.ok) throw std::runtime_error(rigResult.error);
        bundle.placement = std::move(*rigResult.value);

        bundle.atlas_png_path = hitmEngineRoot / "assets" / "parts" / (atlasName + ".png");
        auto dims = ReadPngDimensions(bundle.atlas_png_path);
        if (!dims.ok) throw std::runtime_error(dims.error);
        bundle.atlas_pixel_width = dims.value->first;
        bundle.atlas_pixel_height = dims.value->second;

        // Real, evidenced check (see this file's header comment for why
        // this, and not a sourceSize match): every real part's frame
        // rect must fit inside the atlas's real declared pixel bounds.
        for (const auto& part : bundle.parts.Parts()) {
            int32_t maxX = part.frame_x + part.frame_w;
            int32_t maxY = part.frame_y + part.frame_h;
            if (maxX > bundle.atlas_pixel_width || maxY > bundle.atlas_pixel_height) {
                throw std::runtime_error("part '" + part.name + "' frame rect [" + std::to_string(part.frame_x) + "," +
                                          std::to_string(part.frame_y) + "," + std::to_string(part.frame_w) + "," +
                                          std::to_string(part.frame_h) + "] does not fit within the atlas's real " +
                                          std::to_string(bundle.atlas_pixel_width) + "x" +
                                          std::to_string(bundle.atlas_pixel_height) + " pixel bounds");
            }
        }

        return Result<HitmAssetBundle>::Ok(std::move(bundle));
    } catch (const std::exception& e) {
        return Result<HitmAssetBundle>::Fail("HitmAssetImporter::Import(" + fighterId + "): " + e.what());
    }
}

}  // namespace dominus::character::hitm
