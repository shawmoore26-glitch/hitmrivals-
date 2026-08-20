// GRAPHICS/Raster/PngDecoder.cpp
#include "GRAPHICS/Raster/PngDecoder.h"

#include <array>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>

#include <zlib.h>

namespace dominus::graphics {

namespace {

constexpr std::array<unsigned char, 8> kPngSignature = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};

std::uint32_t ReadU32BE(const unsigned char* p) {
    return (static_cast<std::uint32_t>(p[0]) << 24) | (static_cast<std::uint32_t>(p[1]) << 16) |
           (static_cast<std::uint32_t>(p[2]) << 8) | static_cast<std::uint32_t>(p[3]);
}

// Real PNG spec section 9.2 Paeth predictor -- a, b, c are the real
// already-reconstructed neighbor bytes (left, up, upper-left).
std::uint8_t PaethPredictor(int a, int b, int c) {
    int p = a + b - c;
    int pa = std::abs(p - a);
    int pb = std::abs(p - b);
    int pc = std::abs(p - c);
    if (pa <= pb && pa <= pc) return static_cast<std::uint8_t>(a);
    if (pb <= pc) return static_cast<std::uint8_t>(b);
    return static_cast<std::uint8_t>(c);
}

core::Result<TextureAtlas> Fail(const std::string& msg) { return core::Result<TextureAtlas>::Fail("PngDecoder: " + msg); }

}  // namespace

core::Result<TextureAtlas> DecodePngFile(const std::filesystem::path& pngPath, const std::string& atlasId) {
    std::ifstream file(pngPath, std::ios::binary);
    if (!file) return Fail("cannot open file: " + pngPath.string());
    std::ostringstream contents;
    contents << file.rdbuf();
    return DecodePngBytes(contents.str(), atlasId);
}

core::Result<TextureAtlas> DecodePngBytes(const std::string& pngBytes, const std::string& atlasId) {
    const auto* bytes = reinterpret_cast<const unsigned char*>(pngBytes.data());
    const std::size_t size = pngBytes.size();

    if (size < 8) return Fail("file too small to contain a real PNG signature");
    if (std::memcmp(bytes, kPngSignature.data(), 8) != 0)
        return Fail("bad PNG signature (not a real PNG file, or corrupted)");

    bool sawIHDR = false;
    bool sawIEND = false;
    std::int64_t width = 0, height = 0;
    std::uint8_t bitDepth = 0, colorType = 0, compressionMethod = 0, filterMethod = 0, interlaceMethod = 0;
    std::string idat;  // real, concatenated IDAT payload, in real file order

    std::size_t offset = 8;
    while (offset < size) {
        if (offset + 8 > size) return Fail("truncated chunk header");
        std::uint32_t length = ReadU32BE(bytes + offset);
        std::string type(reinterpret_cast<const char*>(bytes + offset + 4), 4);
        std::size_t dataStart = offset + 8;
        // +4 for the trailing CRC this decoder parses past but does not
        // verify (see this file's header comment).
        if (dataStart + static_cast<std::size_t>(length) + 4 > size)
            return Fail("chunk '" + type + "' runs past end of file (truncated or corrupt)");
        const unsigned char* data = bytes + dataStart;

        if (type == "IHDR") {
            if (sawIHDR) return Fail("multiple IHDR chunks");
            if (length != 13) return Fail("malformed IHDR (wrong length)");
            width = static_cast<std::int64_t>(ReadU32BE(data));
            height = static_cast<std::int64_t>(ReadU32BE(data + 4));
            bitDepth = data[8];
            colorType = data[9];
            compressionMethod = data[10];
            filterMethod = data[11];
            interlaceMethod = data[12];
            sawIHDR = true;
        } else if (!sawIHDR) {
            return Fail("first chunk is not IHDR (a real PNG requires this)");
        } else if (type == "IDAT") {
            idat.append(reinterpret_cast<const char*>(data), length);
        } else if (type == "IEND") {
            sawIEND = true;
        }
        // Any other real, ancillary chunk (pHYs, tEXt, gAMA, ...) is
        // real PNG data this decoder has no use for -- skipped, not an
        // error; this is a texture decoder, not a general PNG metadata
        // tool.

        offset = dataStart + length + 4;
        if (sawIEND) break;
    }

    if (!sawIHDR) return Fail("no IHDR chunk found");
    if (!sawIEND) return Fail("no IEND chunk found (truncated file)");
    if (width <= 0 || height <= 0) return Fail("IHDR declares a non-positive width/height");
    if (compressionMethod != 0) return Fail("unsupported PNG compression method " + std::to_string(compressionMethod));
    if (filterMethod != 0) return Fail("unsupported PNG filter method " + std::to_string(filterMethod));
    if (interlaceMethod != 0)
        return Fail("interlaced PNGs are not supported (no real HITM asset uses interlacing -- see this file's header comment)");
    if (bitDepth != 8)
        return Fail("only 8-bit PNG depth is supported, got " + std::to_string(bitDepth) +
                     " (no real HITM asset uses a different depth)");
    if (colorType != 6)
        return Fail("only PNG color type 6 (truecolor + alpha) is supported, got " + std::to_string(colorType) +
                     " (no real HITM asset uses a different color type)");
    if (idat.empty()) return Fail("no IDAT chunk found (no real pixel data)");

    constexpr int kBytesPerPixel = 4;  // real, fixed for 8-bit RGBA
    const std::size_t rowBytes = static_cast<std::size_t>(width) * kBytesPerPixel;
    const std::size_t expectedRawSize = static_cast<std::size_t>(height) * (rowBytes + 1);  // +1 filter-type byte per row

    std::vector<unsigned char> raw(expectedRawSize);
    uLongf destLen = static_cast<uLongf>(expectedRawSize);
    int zret = uncompress(raw.data(), &destLen, reinterpret_cast<const unsigned char*>(idat.data()),
                           static_cast<uLong>(idat.size()));
    if (zret != Z_OK) return Fail("zlib inflate of the real IDAT stream failed (code " + std::to_string(zret) + ")");
    if (destLen != expectedRawSize)
        return Fail("decompressed size does not match IHDR's real declared width/height (corrupt or truncated data)");

    // Real PNG defiltering (spec section 9): each scanline is prefixed
    // by a real filter-type byte and must be reconstructed against the
    // real, already-reconstructed left/up/upper-left neighbor bytes --
    // the compressed bytes are NOT the real pixel bytes until this
    // runs.
    TextureAtlas atlas;
    atlas.atlas_id = atlasId;
    atlas.width = static_cast<int>(width);
    atlas.height = static_cast<int>(height);
    atlas.rgba.assign(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * kBytesPerPixel, 0);

    std::vector<std::uint8_t> prevRow(rowBytes, 0);
    std::vector<std::uint8_t> curRow(rowBytes, 0);
    std::size_t rawOffset = 0;
    for (std::int64_t y = 0; y < height; ++y) {
        std::uint8_t filterType = raw[rawOffset];
        const unsigned char* filtered = raw.data() + rawOffset + 1;
        for (std::size_t x = 0; x < rowBytes; ++x) {
            std::uint8_t rawByte = filtered[x];
            int a = (x >= static_cast<std::size_t>(kBytesPerPixel)) ? curRow[x - kBytesPerPixel] : 0;  // left
            int b = prevRow[x];                                                                         // up
            int c = (x >= static_cast<std::size_t>(kBytesPerPixel)) ? prevRow[x - kBytesPerPixel] : 0;  // upper-left
            std::uint8_t recon;
            switch (filterType) {
                case 0: recon = rawByte; break;
                case 1: recon = static_cast<std::uint8_t>(rawByte + a); break;
                case 2: recon = static_cast<std::uint8_t>(rawByte + b); break;
                case 3: recon = static_cast<std::uint8_t>(rawByte + static_cast<std::uint8_t>((a + b) / 2)); break;
                case 4: recon = static_cast<std::uint8_t>(rawByte + PaethPredictor(a, b, c)); break;
                default: return Fail("unknown scanline filter type " + std::to_string(filterType));
            }
            curRow[x] = recon;
        }
        std::memcpy(atlas.rgba.data() + static_cast<std::size_t>(y) * rowBytes, curRow.data(), rowBytes);
        prevRow = curRow;
        rawOffset += rowBytes + 1;
    }

    return core::Result<TextureAtlas>::Ok(std::move(atlas));
}

}  // namespace dominus::graphics
