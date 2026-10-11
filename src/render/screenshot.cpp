#include "render/screenshot.h"

#include <glad/gl.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include <cstddef>
#include <string>
#include <vector>

namespace Septhuit
{
    bool saveBackBufferImage(const std::filesystem::path& path, int width, int height)
    {
        if ((width <= 0) || (height <= 0))
        {
            return false;
        }

        // The window's alpha channel carries no meaning; read opaque RGB.
        std::vector<unsigned char> pixels(static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 3U);

        glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
        glReadBuffer(GL_BACK);
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());

        // OpenGL rows start at the bottom; image files start at the top.
        stbi_flip_vertically_on_write(1);
        const std::string file = path.string();
        const int written = (path.extension() == ".bmp")
            ? stbi_write_bmp(file.c_str(), width, height, 3, pixels.data())
            : stbi_write_png(file.c_str(), width, height, 3, pixels.data(), width * 3);
        return written != 0;
    }
}
