#include "render/ui_overlay.h"

#include "render/mesh.h"
#include "render/shader.h"

#include <array>
#include <cctype>
#include <cstddef>

namespace BilliardsSaloon
{
    namespace
    {
        constexpr int GLYPH_ROWS = 7;
        constexpr int GLYPH_COLUMNS = 5;
        constexpr int GLYPH_ADVANCE = 6;

        using GlyphPattern = std::array<const char*, GLYPH_ROWS>;

        void bindOverlayMaterial(
            Shader& shader,
            const MaterialComponent& material,
            const glm::vec3& dynamicEmission)
        {
            shader.setVec3("uMaterialAlbedo", material.albedo);
            shader.setFloat("uMaterialSpecularStrength", material.specularStrength);
            shader.setFloat("uMaterialShininess", material.shininess);
            shader.setInt("uMaterialSurfaceType", static_cast<int>(material.surfaceType));
            shader.setFloat("uMaterialRoughness", material.roughness);
            shader.setFloat("uMaterialReflectivity", material.reflectivity);
            shader.setFloat("uMaterialClearcoatStrength", material.clearcoatStrength);
            shader.setVec3(
                "uEmissionColor",
                material.emissionColor * material.emissionIntensity + dynamicEmission
            );
            shader.setInt("uBallVisualType", 0);
        }

        const GlyphPattern& glyphPatternFor(char character)
        {
            static const GlyphPattern blank = {
                ".....",
                ".....",
                ".....",
                ".....",
                ".....",
                ".....",
                "....."
            };

            static const GlyphPattern dash = {
                ".....",
                ".....",
                ".....",
                "XXXXX",
                ".....",
                ".....",
                "....."
            };

            static const GlyphPattern slash = {
                "....X",
                "...X.",
                "...X.",
                "..X..",
                ".X...",
                ".X...",
                "X...."
            };

            static const GlyphPattern colon = {
                ".....",
                "..X..",
                "..X..",
                ".....",
                "..X..",
                "..X..",
                "....."
            };

            static const GlyphPattern dot = {
                ".....",
                ".....",
                ".....",
                ".....",
                ".....",
                "..X..",
                "..X.."
            };

            static const GlyphPattern zero = {
                ".XXX.",
                "X...X",
                "X..XX",
                "X.X.X",
                "XX..X",
                "X...X",
                ".XXX."
            };

            static const GlyphPattern one = {
                "..X..",
                ".XX..",
                "..X..",
                "..X..",
                "..X..",
                "..X..",
                ".XXX."
            };

            static const GlyphPattern two = {
                ".XXX.",
                "X...X",
                "....X",
                "...X.",
                "..X..",
                ".X...",
                "XXXXX"
            };

            static const GlyphPattern three = {
                "XXXX.",
                "....X",
                "....X",
                ".XXX.",
                "....X",
                "....X",
                "XXXX."
            };

            static const GlyphPattern four = {
                "...X.",
                "..XX.",
                ".X.X.",
                "X..X.",
                "XXXXX",
                "...X.",
                "...X."
            };

            static const GlyphPattern five = {
                "XXXXX",
                "X....",
                "X....",
                "XXXX.",
                "....X",
                "....X",
                "XXXX."
            };

            static const GlyphPattern six = {
                ".XXX.",
                "X....",
                "X....",
                "XXXX.",
                "X...X",
                "X...X",
                ".XXX."
            };

            static const GlyphPattern seven = {
                "XXXXX",
                "....X",
                "...X.",
                "..X..",
                ".X...",
                ".X...",
                ".X..."
            };

            static const GlyphPattern eight = {
                ".XXX.",
                "X...X",
                "X...X",
                ".XXX.",
                "X...X",
                "X...X",
                ".XXX."
            };

            static const GlyphPattern nine = {
                ".XXX.",
                "X...X",
                "X...X",
                ".XXXX",
                "....X",
                "....X",
                ".XXX."
            };

            static const GlyphPattern a = {
                ".XXX.",
                "X...X",
                "X...X",
                "XXXXX",
                "X...X",
                "X...X",
                "X...X"
            };

            static const GlyphPattern b = {
                "XXXX.",
                "X...X",
                "X...X",
                "XXXX.",
                "X...X",
                "X...X",
                "XXXX."
            };

            static const GlyphPattern c = {
                ".XXX.",
                "X...X",
                "X....",
                "X....",
                "X....",
                "X...X",
                ".XXX."
            };

            static const GlyphPattern d = {
                "XXXX.",
                "X...X",
                "X...X",
                "X...X",
                "X...X",
                "X...X",
                "XXXX."
            };

            static const GlyphPattern e = {
                "XXXXX",
                "X....",
                "X....",
                "XXXX.",
                "X....",
                "X....",
                "XXXXX"
            };

            static const GlyphPattern f = {
                "XXXXX",
                "X....",
                "X....",
                "XXXX.",
                "X....",
                "X....",
                "X...."
            };

            static const GlyphPattern g = {
                ".XXX.",
                "X...X",
                "X....",
                "X.XXX",
                "X...X",
                "X...X",
                ".XXX."
            };

            static const GlyphPattern h = {
                "X...X",
                "X...X",
                "X...X",
                "XXXXX",
                "X...X",
                "X...X",
                "X...X"
            };

            static const GlyphPattern i = {
                "XXXXX",
                "..X..",
                "..X..",
                "..X..",
                "..X..",
                "..X..",
                "XXXXX"
            };

            static const GlyphPattern j = {
                "..XXX",
                "...X.",
                "...X.",
                "...X.",
                "...X.",
                "X..X.",
                ".XX.."
            };

            static const GlyphPattern k = {
                "X...X",
                "X..X.",
                "X.X..",
                "XX...",
                "X.X..",
                "X..X.",
                "X...X"
            };

            static const GlyphPattern l = {
                "X....",
                "X....",
                "X....",
                "X....",
                "X....",
                "X....",
                "XXXXX"
            };

            static const GlyphPattern m = {
                "X...X",
                "XX.XX",
                "X.X.X",
                "X...X",
                "X...X",
                "X...X",
                "X...X"
            };

            static const GlyphPattern n = {
                "X...X",
                "XX..X",
                "XX..X",
                "X.X.X",
                "X..XX",
                "X..XX",
                "X...X"
            };

            static const GlyphPattern o = {
                ".XXX.",
                "X...X",
                "X...X",
                "X...X",
                "X...X",
                "X...X",
                ".XXX."
            };

            static const GlyphPattern p = {
                "XXXX.",
                "X...X",
                "X...X",
                "XXXX.",
                "X....",
                "X....",
                "X...."
            };

            static const GlyphPattern q = {
                ".XXX.",
                "X...X",
                "X...X",
                "X...X",
                "X.X.X",
                "X..X.",
                ".XX.X"
            };

            static const GlyphPattern r = {
                "XXXX.",
                "X...X",
                "X...X",
                "XXXX.",
                "X.X..",
                "X..X.",
                "X...X"
            };

            static const GlyphPattern s = {
                ".XXXX",
                "X....",
                "X....",
                ".XXX.",
                "....X",
                "....X",
                "XXXX."
            };

            static const GlyphPattern t = {
                "XXXXX",
                "..X..",
                "..X..",
                "..X..",
                "..X..",
                "..X..",
                "..X.."
            };

            static const GlyphPattern u = {
                "X...X",
                "X...X",
                "X...X",
                "X...X",
                "X...X",
                "X...X",
                ".XXX."
            };

            static const GlyphPattern v = {
                "X...X",
                "X...X",
                "X...X",
                "X...X",
                ".X.X.",
                ".X.X.",
                "..X.."
            };

            static const GlyphPattern w = {
                "X...X",
                "X...X",
                "X...X",
                "X.X.X",
                "X.X.X",
                "XX.XX",
                "X...X"
            };

            static const GlyphPattern x = {
                "X...X",
                ".X.X.",
                ".X.X.",
                "..X..",
                ".X.X.",
                ".X.X.",
                "X...X"
            };

            static const GlyphPattern y = {
                "X...X",
                ".X.X.",
                ".X.X.",
                "..X..",
                "..X..",
                "..X..",
                "..X.."
            };

            static const GlyphPattern z = {
                "XXXXX",
                "....X",
                "...X.",
                "..X..",
                ".X...",
                "X....",
                "XXXXX"
            };

            switch (std::toupper(static_cast<unsigned char>(character)))
            {
                case 'A': return a;
                case 'B': return b;
                case 'C': return c;
                case 'D': return d;
                case 'E': return e;
                case 'F': return f;
                case 'G': return g;
                case 'H': return h;
                case 'I': return i;
                case 'J': return j;
                case 'K': return k;
                case 'L': return l;
                case 'M': return m;
                case 'N': return n;
                case 'O': return o;
                case 'P': return p;
                case 'Q': return q;
                case 'R': return r;
                case 'S': return s;
                case 'T': return t;
                case 'U': return u;
                case 'V': return v;
                case 'W': return w;
                case 'X': return x;
                case 'Y': return y;
                case 'Z': return z;
                case '0': return zero;
                case '1': return one;
                case '2': return two;
                case '3': return three;
                case '4': return four;
                case '5': return five;
                case '6': return six;
                case '7': return seven;
                case '8': return eight;
                case '9': return nine;
                case '-': return dash;
                case '/': return slash;
                case ':': return colon;
                case '.': return dot;
                case ' ': return blank;
                default: return blank;
            }
        }
    }

    float measureUiOverlayTextWidth(const std::string& text, float cellSize)
    {
        if (text.empty())
        {
            return 0.0f;
        }

        return static_cast<float>(
            text.size() * static_cast<std::size_t>(GLYPH_ADVANCE) - 1U
        ) * cellSize;
    }

    void renderUiOverlayBox(
        Shader& shader,
        Mesh& cubeMesh,
        const UiOverlayFrame& frame,
        const glm::vec3& position,
        const glm::vec3& scale,
        const MaterialComponent& material,
        const glm::vec3& dynamicEmission)
    {
        shader.setMat4("uModel", composeMatrix(position, frame.rotation, scale));
        bindOverlayMaterial(shader, material, dynamicEmission);
        cubeMesh.draw();
    }

    void renderUiOverlayText(
        Shader& shader,
        Mesh& cubeMesh,
        const UiOverlayFrame& frame,
        const std::string& text,
        const glm::vec3& anchorPosition,
        const UiTextStyle& style)
    {
        const float textWidth = measureUiOverlayTextWidth(text, style.cellSize);
        const float textHeight = static_cast<float>(GLYPH_ROWS) * style.cellSize;

        float horizontalOffset = 0.0f;
        switch (style.alignment)
        {
            case UiTextAlignment::Left:
                horizontalOffset = 0.0f;
                break;

            case UiTextAlignment::Center:
                horizontalOffset = -0.5f * textWidth;
                break;

            case UiTextAlignment::Right:
                horizontalOffset = -textWidth;
                break;
        }

        const glm::vec3 topLeft =
            anchorPosition +
            frame.right * horizontalOffset +
            frame.up * (0.5f * textHeight);

        const glm::vec3 cellScale(
            style.cellSize * 0.82f,
            style.cellSize * 0.82f,
            style.depth
        );

        for (std::size_t characterIndex = 0; characterIndex < text.size(); ++characterIndex)
        {
            const GlyphPattern& glyph = glyphPatternFor(text[characterIndex]);

            for (int row = 0; row < GLYPH_ROWS; ++row)
            {
                for (int column = 0; column < GLYPH_COLUMNS; ++column)
                {
                    if (glyph[static_cast<std::size_t>(row)][static_cast<std::size_t>(column)] != 'X')
                    {
                        continue;
                    }

                    const glm::vec3 position =
                        topLeft +
                        frame.right * (
                            static_cast<float>(
                                characterIndex * static_cast<std::size_t>(GLYPH_ADVANCE) +
                                static_cast<std::size_t>(column)
                            ) * style.cellSize
                        ) -
                        frame.up * (static_cast<float>(row) * style.cellSize);

                    shader.setMat4("uModel", composeMatrix(position, frame.rotation, cellScale));
                    bindOverlayMaterial(shader, style.material, style.dynamicEmission);
                    cubeMesh.draw();
                }
            }
        }
    }
}
