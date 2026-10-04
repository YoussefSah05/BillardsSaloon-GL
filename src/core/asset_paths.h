#pragma once

#include <filesystem>
#include <string_view>

namespace BilliardsSaloon
{
    // Directory containing the running executable.
    [[nodiscard]] std::filesystem::path executableDirectory();

    // Resolves a path relative to the assets root, independent of the current
    // working directory. Search order:
    //   1. <exe dir>/assets                  (build tree and portable installs)
    //   2. <exe dir>/../Resources/assets     (macOS .app bundle)
    //   3. BS_SOURCE_ASSET_DIR               (source tree, development fallback)
    // Throws std::runtime_error listing every searched location if not found.
    [[nodiscard]] std::filesystem::path resolveAssetPath(std::string_view relativePath);
}
