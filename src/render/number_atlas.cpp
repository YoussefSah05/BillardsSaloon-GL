#include "render/number_atlas.h"

#include <ft2build.h>
#include FT_FREETYPE_H

#include <glad/gl.h>

#include <algorithm>
#include <stdexcept>
#include <string>
#include <vector>

namespace BilliardsSaloon
{
    NumberAtlas::NumberAtlas(const std::filesystem::path& fontFile)
    {
        FT_Library library = nullptr;
        if (FT_Init_FreeType(&library) != 0)
        {
            throw std::runtime_error("FreeType could not start.");
        }
        FT_Face face = nullptr;
        if (FT_New_Face(library, fontFile.string().c_str(), 0, &face) != 0)
        {
            FT_Done_FreeType(library);
            throw std::runtime_error("Could not load the ball number font " + fontFile.string());
        }

        // Digits fill about 70% of a cell's height, like the numbers on a ball's spot.
        const int glyphPixels = CELL * 7 / 10;
        FT_Set_Pixel_Sizes(face, 0, static_cast<FT_UInt>(glyphPixels));

        const int size = GRID * CELL;
        std::vector<unsigned char> pixels(static_cast<std::size_t>(size * size), 0);

        for (int number = 1; number <= 15; ++number)
        {
            const std::string text = std::to_string(number);

            // Measure the string to centre it in its cell.
            int width = 0;
            for (const char c : text)
            {
                FT_Load_Char(face, static_cast<FT_ULong>(c), FT_LOAD_DEFAULT);
                width += static_cast<int>(face->glyph->advance.x >> 6);
            }
            const int ascender = static_cast<int>(face->size->metrics.ascender >> 6);
            const int capHeight = glyphPixels * 7 / 10;   // digits are about cap height

            const int cellX = (number % GRID) * CELL;
            const int cellY = (number / GRID) * CELL;
            int penX = cellX + (CELL - width) / 2;
            const int baseline = cellY + (CELL + capHeight) / 2;
            (void)ascender;

            for (const char c : text)
            {
                if (FT_Load_Char(face, static_cast<FT_ULong>(c), FT_LOAD_RENDER) != 0)
                {
                    continue;
                }
                const FT_GlyphSlot glyph = face->glyph;
                const FT_Bitmap& bitmap = glyph->bitmap;
                for (unsigned int row = 0; row < bitmap.rows; ++row)
                {
                    for (unsigned int col = 0; col < bitmap.width; ++col)
                    {
                        const int x = penX + glyph->bitmap_left + static_cast<int>(col);
                        const int y = baseline - glyph->bitmap_top + static_cast<int>(row);
                        if ((x < cellX) || (x >= cellX + CELL) || (y < cellY) || (y >= cellY + CELL))
                        {
                            continue;
                        }
                        unsigned char& out = pixels[static_cast<std::size_t>(y * size + x)];
                        out = std::max(out, bitmap.buffer[row * static_cast<unsigned int>(bitmap.pitch) + col]);
                    }
                }
                penX += static_cast<int>(glyph->advance.x >> 6);
            }
        }

        FT_Done_Face(face);
        FT_Done_FreeType(library);

        // Rows were written top-down; the shader flips v to match.
        glGenTextures(1, &m_texture);
        glBindTexture(GL_TEXTURE_2D, m_texture);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, size, size, 0, GL_RED, GL_UNSIGNED_BYTE, pixels.data());
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
        glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }

    NumberAtlas::~NumberAtlas()
    {
        glDeleteTextures(1, &m_texture);
    }
}
