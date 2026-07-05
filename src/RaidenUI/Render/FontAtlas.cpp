#include <RaidenUI/Render/FontAtlas.hpp>

#include <Raiden/Core/IVirtualFileSystem.hpp>
#include <Raiden/Renderer/RenderTypes.hpp>

#include <algorithm>
#include <cstdint>
#include <vector>

#include <ft2build.h>

#include FT_FREETYPE_H

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

  FT_Library ftLibrary = nullptr;
  if (FT_Init_FreeType(&ftLibrary) != 0) {
    return;
  }

  FT_Face face = nullptr;
  if (FT_New_Memory_Face(
          ftLibrary, reinterpret_cast<const FT_Byte *>(fontData.data()),
          static_cast<FT_Long>(fontData.size()), 0, &face) != 0) {
    FT_Done_FreeType(ftLibrary);
    return;
  }

  FT_Set_Pixel_Sizes(face, 0, static_cast<FT_UInt>(fontSize));

  m_ascent = static_cast<float>(face->size->metrics.ascender) / 64.0F;
  m_descent = static_cast<float>(face->size->metrics.descender) / 64.0F;
  m_lineHeight = static_cast<float>(face->size->metrics.height) / 64.0F;

  std::vector<PackedGlyph> packed;
  int cursorX = kPadding;
  int cursorY = kPadding;
  int rowHeight = 0;

  for (char32_t cp = 32; cp <= 126; ++cp) {
    FT_UInt glyphIndex = FT_Get_Char_Index(face, static_cast<FT_ULong>(cp));
    if (glyphIndex == 0) {
      continue;
    }

    if (FT_Load_Glyph(face, glyphIndex,
                       FT_LOAD_DEFAULT | FT_LOAD_TARGET_NORMAL) != 0) {
      continue;
    }

    float advance =
        static_cast<float>(face->glyph->metrics.horiAdvance) / 64.0F;
    float bearingX =
        static_cast<float>(face->glyph->metrics.horiBearingX) / 64.0F;

    if (FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL) != 0) {
      PackedGlyph pg;
      pg.codepoint = cp;
      pg.advance = advance;
      pg.bearingX = bearingX;
      pg.bearingY = 0.0F;
      packed.push_back(pg);
      continue;
    }

    int gw = static_cast<int>(face->glyph->bitmap.width);
    int gh = static_cast<int>(face->glyph->bitmap.rows);

    if (gw <= 0 || gh <= 0) {
      PackedGlyph pg;
      pg.codepoint = cp;
      pg.advance = advance;
      pg.bearingX = bearingX;
      pg.bearingY = static_cast<float>(-face->glyph->bitmap_top);
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

    int pitch = face->glyph->bitmap.pitch;
    if (pitch < 0) {
      pitch = -pitch;
    }

    std::vector<uint8_t> bitmap(static_cast<size_t>(gw) *
                                 static_cast<size_t>(gh));

    for (int y = 0; y < gh; ++y) {
      const uint8_t *srcRow =
          face->glyph->bitmap.buffer + (static_cast<size_t>(y) * pitch);
      uint8_t *dstRow = bitmap.data() + (static_cast<size_t>(y) * gw);
      std::copy(srcRow, srcRow + gw, dstRow);
    }

    PackedGlyph pg;
    pg.codepoint = cp;
    pg.x = cursorX;
    pg.y = cursorY;
    pg.w = gw;
    pg.h = gh;
    pg.advance = advance;
    pg.bearingX = bearingX;
    pg.bearingY = static_cast<float>(-face->glyph->bitmap_top);
    pg.bitmap = std::move(bitmap);
    packed.push_back(pg);

    cursorX += gw + kPadding;
    rowHeight = std::max(rowHeight, gh);
  }

  FT_Done_Face(face);
  FT_Done_FreeType(ftLibrary);

  std::vector<uint8_t> atlas(static_cast<size_t>(kAtlasWidth) *
                                 static_cast<size_t>(kAtlasHeight) * 4,
                             0);

  for (const auto &pg : packed) {
    GlyphInfo gi;
    gi.advance = pg.advance;
    gi.bearingX = pg.bearingX;
    gi.bearingY = pg.bearingY;
    gi.width = static_cast<float>(pg.w);
    gi.height = static_cast<float>(pg.h);

    if (pg.w > 0 && pg.h > 0 && !pg.bitmap.empty()) {
      for (int y = 0; y < pg.h; ++y) {
        for (int x = 0; x < pg.w; ++x) {
          uint8_t coverage =
              pg.bitmap[(static_cast<size_t>(y) * static_cast<size_t>(pg.w)) +
                        static_cast<size_t>(x)];
          size_t dstIdx = ((static_cast<size_t>(pg.y + y) *
                            static_cast<size_t>(kAtlasWidth)) +
                           static_cast<size_t>(pg.x + x)) *
                          4;
          atlas[dstIdx + 0] = 255;
          atlas[dstIdx + 1] = 255;
          atlas[dstIdx + 2] = 255;
          atlas[dstIdx + 3] = coverage;
        }
      }

      gi.u0 = static_cast<float>(pg.x) / static_cast<float>(kAtlasWidth);
      gi.v0 = static_cast<float>(pg.y) / static_cast<float>(kAtlasHeight);
      gi.u1 = static_cast<float>(pg.x + pg.w) / static_cast<float>(kAtlasWidth);
      gi.v1 = static_cast<float>(pg.y + pg.h) / static_cast<float>(kAtlasHeight);
    }

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