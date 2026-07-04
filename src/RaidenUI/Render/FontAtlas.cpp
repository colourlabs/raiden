#include <RaidenUI/Render/FontAtlas.hpp>

#include <Raiden/Core/IVirtualFileSystem.hpp>
#include <Raiden/Renderer/RenderTypes.hpp>

#include <algorithm>
#include <cstdint>
#include <vector>

#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>

namespace RaidenUI {

namespace {

constexpr int kAtlasWidth = 512;
constexpr int kAtlasHeight = 512;
constexpr int kPadding = 2;

struct PackedGlyph {
  char32_t codepoint;
  int x{0}, y{0}, w{0}, h{0};
  float advance{0};
  float bearingX{0};
  float bearingY{0};
  std::vector<uint8_t> bitmap;
};

} // namespace

FontAtlas::FontAtlas(Raiden::Renderer::IRenderDevice &device,
                     Raiden::Core::IVirtualFileSystem &vfs,
                     std::string_view fontPath, float fontSize)
    : m_fontSize(fontSize) {
  auto fontData = vfs.readBytes(fontPath);
  if (fontData.empty()) {
    return;
  }

  stbtt_fontinfo info;
  if (stbtt_InitFont(&info, reinterpret_cast<const uint8_t*>(fontData.data()), 0) == 0) {
    return;
  }

  float scale = stbtt_ScaleForPixelHeight(&info, fontSize);

  int ascent = 0, descent = 0, lineGap = 0;
  stbtt_GetFontVMetrics(&info, &ascent, &descent, &lineGap);

  m_ascent = static_cast<float>(ascent) * scale;
  m_descent = static_cast<float>(descent) * scale;
  m_lineHeight = m_ascent - m_descent + (static_cast<float>(lineGap) * scale);

  std::vector<PackedGlyph> packed;
  int cursorX = kPadding;
  int cursorY = kPadding;
  int rowHeight = 0;

  for (char32_t cp = 32; cp <= 126; ++cp) {
    int glyphIndex = stbtt_FindGlyphIndex(&info, static_cast<int>(cp));
    if (glyphIndex == 0) {
      continue;
    }

    int advance = 0, bearingX = 0;
    stbtt_GetGlyphHMetrics(&info, glyphIndex, &advance, &bearingX);

    int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    stbtt_GetGlyphBitmapBox(&info, glyphIndex, scale, scale, &x0, &y0, &x1,
                            &y1);

    int gw = x1 - x0;
    int gh = y1 - y0;

    if (gw <= 0 || gh <= 0) {
      PackedGlyph pg;
      pg.codepoint = cp;
      pg.advance = static_cast<float>(advance) * scale;
      pg.bearingX = static_cast<float>(bearingX) * scale;
      pg.bearingY = static_cast<float>(y0);
      packed.push_back(pg);
      continue;
    }

    if (cursorX + gw + kPadding > kAtlasWidth) {
      cursorX = kPadding;
      cursorY += rowHeight + kPadding;
      rowHeight = 0;
    }

    if (cursorY + gh + kPadding > kAtlasHeight) {
      break;
    }

    std::vector<uint8_t> bitmap(static_cast<size_t>(gw) *
                                static_cast<size_t>(gh));
    stbtt_MakeGlyphBitmap(&info, bitmap.data(), gw, gh, gw, scale, scale,
                          glyphIndex);

    PackedGlyph pg;
    pg.codepoint = cp;
    pg.x = cursorX;
    pg.y = cursorY;
    pg.w = gw;
    pg.h = gh;
    pg.advance = static_cast<float>(advance) * scale;
    pg.bearingX = static_cast<float>(bearingX) * scale;
    pg.bearingY = static_cast<float>(y0);
    pg.bitmap = std::move(bitmap);
    packed.push_back(pg);

    cursorX += gw + kPadding;
    rowHeight = std::max(rowHeight, gh);
  }

  std::vector<uint8_t> atlas(static_cast<size_t>(kAtlasWidth) *
                                 static_cast<size_t>(kAtlasHeight) * 4,
                             0);

  for (const auto &pg : packed) {
    if (pg.w <= 0 || pg.h <= 0 || pg.bitmap.empty()) {
      continue;
    }

    for (int y = 0; y < pg.h; ++y) {
      for (int x = 0; x < pg.w; ++x) {
        uint8_t alpha =
            pg.bitmap[(static_cast<size_t>(y) * static_cast<size_t>(pg.w)) +
                      static_cast<size_t>(x)];
        size_t dstIdx = ((static_cast<size_t>(pg.y + y) *
                          static_cast<size_t>(kAtlasWidth)) +
                         static_cast<size_t>(pg.x + x)) *
                        4;
        atlas[dstIdx + 0] = 255;
        atlas[dstIdx + 1] = 255;
        atlas[dstIdx + 2] = 255;
        atlas[dstIdx + 3] = alpha;
      }
    }

    GlyphInfo gi;
    gi.advance = pg.advance;
    gi.bearingX = pg.bearingX;
    gi.bearingY = pg.bearingY;
    gi.width = static_cast<float>(pg.w);
    gi.height = static_cast<float>(pg.h);
    gi.u0 = static_cast<float>(pg.x) / static_cast<float>(kAtlasWidth);
    gi.v0 = static_cast<float>(pg.y) / static_cast<float>(kAtlasHeight);
    gi.u1 = static_cast<float>(pg.x + pg.w) / static_cast<float>(kAtlasWidth);
    gi.v1 = static_cast<float>(pg.y + pg.h) / static_cast<float>(kAtlasHeight);
    m_glyphs[pg.codepoint] = gi;
  }

  Raiden::Renderer::TextureDesc desc;
  desc.width = kAtlasWidth;
  desc.height = kAtlasHeight;
  desc.format = Raiden::Renderer::Format::R8G8B8A8_UNORM;
  desc.type = Raiden::Renderer::TextureType::Texture2D;

  m_texture = device.createTexture(desc);
  if (m_texture != nullptr) {
    m_texture->upload(atlas.data(), atlas.size());
  }
}

const GlyphInfo *FontAtlas::glyph(char32_t codepoint) const {
  auto it = m_glyphs.find(codepoint);
  if (it != m_glyphs.end()) {
    return &it->second;
  }
  return nullptr;
}

} // namespace RaidenUI
