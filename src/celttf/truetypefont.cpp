// truetypefont.cpp
//
// Copyright (C) 2019, Celestia Development Team
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.

#include <algorithm>
#include <array>
#include <iostream>
#include <vector>
#include <fmt/printf.h>
#include <celutil/utf8.h>
#include <celengine/glsupport.h>
#include <celengine/render.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include "truetypefont.h"

#define DUMP_TEXTURE 0

#if DUMP_TEXTURE
#include <fstream>
#endif

using namespace std;

static FT_Library ft = nullptr;

struct Glyph
{
    wchar_t ch;

    int ax;    // advance.x
    int ay;    // advance.y

    int bw;    // bitmap.width;
    int bh;    // bitmap.height;

    int bl;    // bitmap_left;
    int bt;    // bitmap_top;

    float tx;  // x offset of glyph in texture coordinates
    float ty;  // y offset of glyph in texture coordinates
};

struct UnicodeBlock
{
    wchar_t first, last;
};

struct TextureFontPrivate
{
    struct FontVertex
    {
        FontVertex(float _x, float _y, float _u, float _v) :
            x(_x), y(_y), u(_u), v(_v)
        {}
        float x, y;
        float u, v;
    };

    TextureFontPrivate() = delete;
    TextureFontPrivate(const Renderer *renderer);
    ~TextureFontPrivate();
    TextureFontPrivate(const TextureFontPrivate&) = default;
    TextureFontPrivate(TextureFontPrivate&&) = default;
    TextureFontPrivate& operator=(const TextureFontPrivate&) = default;
    TextureFontPrivate& operator=(TextureFontPrivate&&) = default;

    float render(const string &s, float x, float y);
    float render(wchar_t ch, float xoffset, float yoffset);

    bool buildAtlas();
    void computeTextureSize();
    bool loadGlyphInfo(wchar_t, Glyph&);
    void initCommonGlyphs();
    int getCommonGlyphsCount();
    Glyph& getGlyph(wchar_t);
    Glyph& getGlyph(wchar_t, wchar_t);
    int toPos(wchar_t) const;
    void optimize();
    CelestiaGLProgram* getProgram();
    void flush();

    const Renderer *m_renderer;
    CelestiaGLProgram *m_prog { nullptr };

    FT_Face m_face;         // font face

    int m_maxAscent;
    int m_maxDescent;
    int m_maxWidth;

    int m_texWidth;
    int m_texHeight;

    GLuint m_texName;       // texture object
    vector<Glyph> m_glyphs; // character information
    GLint m_maxTextureSize; // max supported texture size

    array<UnicodeBlock, 2> m_unicodeBlocks;
    int m_commonGlyphsCount { 0 };

    int m_inserted { 0 };

    Eigen::Matrix4f m_MVP;
    bool m_shaderInUse { false };
    vector<FontVertex> m_fontVertices;
};

inline float pt_to_px(float pt, int dpi = 96)
{
   return dpi == 0 ? pt : pt / 72.0 * dpi;
}

/*
   first = ((c / 32) + 1) * 32  == c & ~0xdf
   last  = first + 32
*/

TextureFontPrivate::TextureFontPrivate(const Renderer *renderer) :
    m_renderer(renderer)
{
    m_unicodeBlocks[0] = { 0x0020, 0x007E }; // Basic Latin
    m_unicodeBlocks[1] = { 0x03B1, 0x03CF }; // Lower case Greek

    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &m_maxTextureSize);
}

TextureFontPrivate::~TextureFontPrivate()
{
    if (m_face)
        FT_Done_Face(m_face);
    if (m_texName != 0)
        glDeleteTextures(1, &m_texName);
}

bool TextureFontPrivate::loadGlyphInfo(wchar_t ch, Glyph &c)
{
    FT_GlyphSlot g = m_face->glyph;
    if (FT_Load_Char(m_face, ch, FT_LOAD_RENDER) != 0)
    {
        c.ch = 0;
        return false;
    }

    c.ch = ch;
    c.ax = g->advance.x >> 6;
    c.ay = g->advance.y >> 6;
    c.bw = g->bitmap.width;
    c.bh = g->bitmap.rows;
    c.bl = g->bitmap_left;
    c.bt = g->bitmap_top;
    return true;
}

void TextureFontPrivate::initCommonGlyphs()
{
    if (m_glyphs.size() > 0)
        return;

    m_glyphs.reserve(256);

    for (auto const &block : m_unicodeBlocks)
    {
        for (wchar_t ch = block.first, e = block.last; ch <= e; ch++)
        {
            Glyph c;
            if (!loadGlyphInfo(ch, c))
                fmt::fprintf(cerr, "Loading character %x failed!\n", (unsigned)ch);
            m_glyphs.push_back(c); // still pushing empty
        }
    }
}

void TextureFontPrivate::computeTextureSize()
{
    FT_GlyphSlot g = m_face->glyph;

    int roww = 0;
    int rowh = 0;
    int w = 0;
    int h = 0;

    // Find minimum size for a texture holding all visible ASCII characters
    for (const auto &c : m_glyphs)
    {
        if (c.ch == 0) continue; // skip bad glyphs

        if (roww + c.bw + 1 >= m_maxTextureSize)
        {
            w = max(w, roww);
            h += rowh;
            roww = 0;
            rowh = 0;
        }
        roww += c.bw + 1;
        rowh = max(rowh, (int)c.bh);
    }

    w = max(w, roww);
    h += rowh;

    m_texWidth = w;
    m_texHeight = h;
}

bool TextureFontPrivate::buildAtlas()
{
    FT_GlyphSlot g = m_face->glyph;
    Glyph c;

    initCommonGlyphs();
    computeTextureSize();

    // Create a texture that will be used to hold all glyphs
    glActiveTexture(GL_TEXTURE0);
    if (m_texName != 0)
        glDeleteTextures(1, &m_texName);
    glGenTextures(1, &m_texName);
    if (m_texName == 0)
        return false;

    glBindTexture(GL_TEXTURE_2D, m_texName);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, m_texWidth, m_texHeight, 0, GL_ALPHA, GL_UNSIGNED_BYTE, 0);

    // We require 1 byte alignment when uploading texture data
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    // Clamping to edges is important to prevent artifacts when scaling
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Linear filtering usually looks best for text
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Paste all glyph bitmaps into the texture, remembering the offset
    int ox = 0;
    int oy = 0;

    int rowh = 0;

    for (auto &c : m_glyphs)
    {
        if (c.ch == 0) continue; // skip bad glyphs

        if (FT_Load_Char(m_face, c.ch, FT_LOAD_RENDER))
        {
           fmt::fprintf(cerr, "Loading character %x failed!\n", (unsigned)c.ch);
           c.ch = 0;
           continue;
        }

        if (ox + g->bitmap.width > m_texWidth)
        {
            oy += rowh;
            rowh = 0;
            ox = 0;
        }

        glTexSubImage2D(GL_TEXTURE_2D, 0, ox, oy, g->bitmap.width, g->bitmap.rows, GL_ALPHA, GL_UNSIGNED_BYTE, g->bitmap.buffer);
        c.tx = (float)ox / (float)m_texWidth;
        c.ty = (float)oy / (float)m_texHeight;

        rowh = max(rowh, (int)g->bitmap.rows);
        ox += g->bitmap.width + 1;
    }

#if DUMP_TEXTURE
    fmt::fprintf(cout/*cerr*/, "Generated a %d x %d (%d kb) texture atlas\n", m_texWidth, m_texHeight, m_texWidth * m_texHeight / 1024);
    size_t img_size = sizeof(uint8_t) * m_texWidth * m_texHeight * 4;
    uint8_t *raw_img = new uint8_t[img_size];
    glGetTexImage(GL_TEXTURE_2D, 0, GL_BGRA, GL_UNSIGNED_BYTE, raw_img);
    ofstream f(fmt::sprintf("/tmp/texture_%ix%i.data", m_texWidth, m_texHeight), ios::binary);
    f.write(reinterpret_cast<char*>(raw_img), img_size);
    f.close();
    delete[] raw_img;
#endif
    return true;
}

int TextureFontPrivate::getCommonGlyphsCount()
{
    if (m_commonGlyphsCount == 0)
    {
        for (auto const &block : m_unicodeBlocks)
            m_commonGlyphsCount += (block.last - block.first + 1);
    }
    return m_commonGlyphsCount;
}

int TextureFontPrivate::toPos(wchar_t ch) const
{
    int pos = 0;

    if (ch > m_unicodeBlocks.back().last)
        return -1;

    for (const auto &r : m_unicodeBlocks)
    {
        if (ch < r.first)
            return -1;

        if (ch <= r.last)
        {
            return pos + ch - r.first;
        }

        pos += r.last - r.first + 1;
    }
    return -1;
}

Glyph& TextureFontPrivate::getGlyph(wchar_t ch, wchar_t fallback)
{
    auto &g = getGlyph(ch);
    return g.ch == ch ? g : getGlyph(fallback);
}

Glyph g_badGlyph = {0};
Glyph& TextureFontPrivate::getGlyph(wchar_t ch)
{
    auto pos = toPos(ch);
    if (pos != -1)
        return m_glyphs[pos];

    auto it = find_if(m_glyphs.begin() + getCommonGlyphsCount(),
                      m_glyphs.end(),
                      [ch](Glyph &g) { return g.ch == ch; });

    if (it != m_glyphs.end())
        return *it;

    Glyph c;
    if (!loadGlyphInfo(ch, c))
        return g_badGlyph;

    m_glyphs.push_back(c);
    if (++m_inserted == 10)
        optimize();
    buildAtlas();

    return m_glyphs.back();
}

void TextureFontPrivate::optimize()
{
    m_inserted = 0;
}


struct FontVertex
{
    FontVertex(float _x, float _y, float _u, float _v) :
        x(_x), y(_y), u(_u), v(_v)
    {}
    float x, y;
    float u, v;
};
/*
 * Render text using the currently loaded font and currently set font size.
 * Rendering starts at coordinates (x, y), z is always 0.
 * The pixel coordinates that the FreeType2 library uses are scaled by (sx, sy).
 */
float TextureFontPrivate::render(const string &s, float x, float y)
{
    if (m_texName == 0)
        return 0;

    // Use the texture containing the atlas
    glBindTexture(GL_TEXTURE_2D, m_texName);

    // Loop through all characters
    int len = s.length();
    bool validChar = true;
    int i = 0;

    while (i < len && validChar)
    {
        wchar_t ch = 0;
        validChar = UTF8Decode(s, i, ch);
        if (!validChar)
            break;
        i += UTF8EncodedSize(ch);

        auto& g = getGlyph(ch, L'?');

        // Calculate the vertex and texture coordinates
        const float x1 = x + g.bl;
        const float y1 = y + g.bt - g.bh;
        const float w = g.bw;
        const float h = g.bh;
        const float x2 = x1 + w;
        const float y2 = y1 + h;

        // Advance the cursor to the start of the next character
        x += g.ax;
        y += g.ay;

        // Skip glyphs that have no pixels
        if (g.bw == 0 || g.bh == 0)
            continue;

        const float tx1 = g.tx;
        const float ty1 = g.ty;
        const float tx2 = tx1 + w / m_texWidth;
        const float ty2 = ty1 + h / m_texHeight;

        m_fontVertices.emplace_back(FontVertex(x1, y1, tx1, ty2));
        m_fontVertices.emplace_back(FontVertex(x2, y1, tx2, ty2));
        m_fontVertices.emplace_back(FontVertex(x1, y2, tx1, ty1));
        m_fontVertices.emplace_back(FontVertex(x2, y2, tx2, ty1));
    }

    return x;
}

float TextureFontPrivate::render(wchar_t ch, float xoffset, float yoffset)
{

    auto& g = getGlyph(ch, L'?');

    // Calculate the vertex and texture coordinates
    const float x1 = xoffset + g.bl;
    const float y1 = yoffset + g.bt - g.bh;
    const float x2 = x1 + g.bw;
    const float y2 = y1 + g.bh;

    const float tx1 = g.tx;
    const float ty1 = g.ty;
    const float tx2 = tx1 + static_cast<float>(g.bw) / m_texWidth;
    const float ty2 = ty1 + static_cast<float>(g.bh) / m_texHeight;

    m_fontVertices.emplace_back(FontVertex(x1, y1, tx1, ty2));
    m_fontVertices.emplace_back(FontVertex(x2, y1, tx2, ty2));
    m_fontVertices.emplace_back(FontVertex(x1, y2, tx1, ty1));
    m_fontVertices.emplace_back(FontVertex(x2, y2, tx2, ty1));

    return g.ax;
}

CelestiaGLProgram* TextureFontPrivate::getProgram()
{
    if (m_prog != nullptr)
        return m_prog;
    m_prog = m_renderer->getShaderManager().getShader("text");
    return m_prog;
}

void TextureFontPrivate::flush()
{
    if (m_fontVertices.size() < 4)
        return;

    vector<unsigned short> indexes;
    indexes.reserve(m_fontVertices.size() / 4 * 6);
    for (unsigned short index = 0;
         index < (unsigned short) m_fontVertices.size();
         index += 4)
    {
        indexes.push_back(index + 0);
        indexes.push_back(index + 1);
        indexes.push_back(index + 2);
        indexes.push_back(index + 1);
        indexes.push_back(index + 3);
        indexes.push_back(index + 2);
    }

    glEnableVertexAttribArray(CelestiaGLProgram::VertexCoordAttributeIndex);
    glEnableVertexAttribArray(CelestiaGLProgram::TextureCoord0AttributeIndex);
    glVertexAttribPointer(CelestiaGLProgram::VertexCoordAttributeIndex,
                          2, GL_FLOAT, GL_FALSE, sizeof(FontVertex), &m_fontVertices[0].x);
    glVertexAttribPointer(CelestiaGLProgram::TextureCoord0AttributeIndex,
                          2, GL_FLOAT, GL_FALSE, sizeof(FontVertex), &m_fontVertices[0].u);
    glDrawElements(GL_TRIANGLES, indexes.size(), GL_UNSIGNED_SHORT, indexes.data());
    glDisableVertexAttribArray(CelestiaGLProgram::VertexCoordAttributeIndex);
    glDisableVertexAttribArray(CelestiaGLProgram::TextureCoord0AttributeIndex);

    m_fontVertices.clear();
}


TextureFont::TextureFont(const Renderer *renderer) :
    impl(new TextureFontPrivate(renderer))
{
}

TextureFont::~TextureFont()
{
    delete impl;
}

/**
 * Render a single character of the font with offset
 *
 * Render a single character of the font, adding the specified offset
 * to the location. Do *not* automatically update the modelview transform.
 *
 * @param ch -- wide character
 * @param xoffset -- horizontal offset
 * @param yoffset -- vertical offset
 */
float TextureFont::render(wchar_t ch, float xoffset, float yoffset) const
{
    return impl->render(ch, xoffset, yoffset);
}

/**
 * Render a string with the specified offset
 *
 * Render a string with the specified offset. Do *not* automatically update
 * the modelview transform.
 *
 * @param s -- string to render
 * @param xoffset -- horizontal offset
 * @param yoffset -- vertical offset
 */
float TextureFont::render(const string &s, float xoffset, float yoffset) const
{
    return impl->render(s, xoffset, yoffset);
}

/**
 * Calculate string width in pixels
 *
 * Calculate string width using the current font.
 *
 * @param s -- string to calculate width
 * @return string width in pixels
 */
int TextureFont::getWidth(const string& s) const
{
    int width = 0;
    int len = s.length();
    bool validChar = true;
    int i = 0;

    while (i < len && validChar)
    {
        wchar_t ch = 0;
        validChar = UTF8Decode(s, i, ch);
        if (!validChar)
            break;

        i += UTF8EncodedSize(ch);

        auto& g = impl->getGlyph(ch, L'?');
        width += g.ax;
    }

    return width;
}

int TextureFont::getHeight() const
{
    return impl->m_maxAscent + impl->m_maxDescent;
}

int TextureFont::getMaxWidth() const
{
    return impl->m_maxWidth;
}

int TextureFont::getMaxAscent() const
{
    return impl->m_maxAscent;
}

void TextureFont::setMaxAscent(int _maxAscent)
{
    impl->m_maxAscent = _maxAscent;
}

int TextureFont::getMaxDescent() const
{
    return impl->m_maxDescent;
}

void TextureFont::setMaxDescent(int _maxDescent)
{
    impl->m_maxDescent = _maxDescent;
}

int TextureFont::getTextureName() const
{
    return impl->m_texName;
}

void TextureFont::bind()
{
    auto *prog = impl->getProgram();
    if (prog == nullptr)
        return;

    if (impl->m_texName != 0)
    {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, impl->m_texName);
        prog->use();
        prog->samplerParam("atlasTex") = 0;
        impl->m_shaderInUse = true;
        prog->mat4Param("MVPMatrix") = impl->m_MVP;
    }
}

void TextureFont::setMVPMatrix(const Eigen::Matrix4f& mvp)
{
    impl->m_MVP = mvp;
    auto *prog = impl->getProgram();
    if (prog != nullptr && impl->m_shaderInUse)
    {
        flush();
        prog->mat4Param("MVPMatrix") = mvp;
    }
}

void TextureFont::unbind()
{
    flush();
    impl->m_shaderInUse = false;
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0);
}

short TextureFont::getAdvance(wchar_t ch) const
{
    auto& g = impl->getGlyph(ch, L'?');
    return g.ax;
}

bool TextureFont::buildTexture()
{
    return true;
}

void TextureFont::flush()
{
    impl->flush();
}

TextureFont* TextureFont::load(const Renderer *r, const fs::path &path, int index, int size, int dpi)
{
    FT_Face face;

    if (FT_New_Face(ft, path.string().c_str(), index, &face) != 0)
    {
        fmt::fprintf(cerr, "Could not open font %s\n", path);
        return nullptr;
    }

    if (!FT_IS_SCALABLE(face))
    {
        fmt::fprintf(cerr, "Font is not scalable: %s\n", path);
        return nullptr;
    }

    if (FT_Set_Char_Size(face, 0, size << 6, dpi, dpi) != 0)
    {
        fmt::fprintf(cerr, "Could not font size %i\n", size);
        return nullptr;
    }

    auto* font = new TextureFont(r);
    font->impl->m_face = face;

    if (!font->impl->buildAtlas())
        return nullptr;

    font->setMaxAscent(face->size->metrics.ascender >> 6);
    font->setMaxDescent(-face->size->metrics.descender >> 6);

    return font;
}

// temporary while no fontconfig support
static fs::path ParseFontName(const fs::path &filename, int &size)
{
    auto fn = filename.string();
    auto pos = fn.rfind(',');
    if (pos != string::npos)
    {
        size = (int) stof(fn.substr(pos + 1));
        return fn.substr(0, pos);
    }
    else
    {
        size = 12;
        return filename;
    }
}

TextureFont* LoadTextureFont(const Renderer *r, const fs::path &filename, int index, int size, int dpi)
{
    if (ft == nullptr)
    {
        if (FT_Init_FreeType(&ft))
        {
            cerr << "Could not init freetype library\n";
            return nullptr;
        }
    }

    int psize = 0;
    auto nameonly = ParseFontName(filename, psize);
    return TextureFont::load(r, nameonly, index, size > 0 ? size : psize, dpi);
}
