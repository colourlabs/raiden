#pragma once

#include <Raiden/Renderer/IRenderDevice.hpp>
#include <Raiden/Renderer/ITexture.hpp>

#include <memory>
#include <string_view>
#include <unordered_map>

namespace RaidenUI {

struct GlyphInfo {
  float advance;
  float bearingX;
  float bearingY;
  float width;
  float height;
  float u0;
  float v0;
  float u1;
  float v1;
};

class FontAtlas {
public:
  FontAtlas(Raiden::Renderer::IRenderDevice &device, std::string_view fontPath,
            float fontSize);

  explicit operator bool() const { return m_texture != nullptr; }

  const GlyphInfo *glyph(char32_t codepoint) const;
  Raiden::Renderer::ITexture *texture() const { return m_texture.get(); }
  float fontSize() const { return m_fontSize; }
  float ascent() const { return m_ascent; }
  float descent() const { return m_descent; }
  float lineHeight() const { return m_lineHeight; }

private:
  std::unique_ptr<Raiden::Renderer::ITexture> m_texture;
  std::unordered_map<char32_t, GlyphInfo> m_glyphs;
  float m_fontSize{0};
  float m_ascent{0};
  float m_descent{0};
  float m_lineHeight{0};
};

} // namespace RaidenUI
