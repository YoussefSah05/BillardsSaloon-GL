#include "render/screenshot.h"

#include <glad/gl.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include <cstddef>
#include <vector>

namespace BilliardsSaloon
{
    bool saveBackBufferPng(const std::filesystem::path& path, int width, int height)
    {
        if ((width <= 0) || (height <= 0))
        {
            return false;
        }

        std::vector<unsigned char> pixels(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4U);

        glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
        glReadBuffer(GL_BACK);
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());

        // The window's alpha channel carries no meaning; save an opaque image.
        for (std::size_t i = 3; i < pixels.size(); i += 4)
        {
            pixels[i] = 255;
        }

        // OpenGL rows start at the bottom; PNG rows start at the top.
        stbi_flip_vertically_on_write(1);
        const int written = stbi_write_png(path.string().c_str(), width, height, 4, pixels.data(), width * 4);
        return written != 0;
    }
}
