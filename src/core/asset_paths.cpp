#include "core/asset_paths.h"

#include <cstdint>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

#if defined(_WIN32)
    #define WIN32_LEAN_AND_MEAN
    #define NOMINMAX
    #include <windows.h>
#elif defined(__APPLE__)
    #include <mach-o/dyld.h>
#endif

namespace BilliardsSaloon
{
    namespace
    {
        [[nodiscard]] std::filesystem::path queryExecutablePath()
        {
#if defined(_WIN32)
            std::wstring buffer(MAX_PATH, L'\0');

            for (;;)
            {
                const DWORD length = GetModuleFileNameW(
                    nullptr,
                    buffer.data(),
                    static_cast<DWORD>(buffer.size())
                );

                if (length == 0)
                {
                    return {};
                }

                if (length < buffer.size())
                {
                    buffer.resize(length);
                    return std::filesystem::path(buffer);
                }

                buffer.resize(buffer.size() * 2);
            }
#elif defined(__APPLE__)
            std::uint32_t size = 0;
            _NSGetExecutablePath(nullptr, &size);

            std::string buffer(size, '\0');
            if (_NSGetExecutablePath(buffer.data(), &size) != 0)
            {
                return {};
            }

            buffer.resize(std::char_traits<char>::length(buffer.c_str()));
            return std::filesystem::path(buffer);
#else
            std::error_code error;
            const std::filesystem::path path = std::filesystem::read_symlink("/proc/self/exe", error);
            return error ? std::filesystem::path{} : path;
#endif
        }
    }

    std::filesystem::path executableDirectory()
    {
        static const std::filesystem::path directory = []()
        {
            std::filesystem::path path = queryExecutablePath();

            std::error_code error;
            const std::filesystem::path canonical = std::filesystem::weakly_canonical(path, error);
            if (!error)
            {
                path = canonical;
            }

            return path.parent_path();
        }();

        return directory;
    }

    std::filesystem::path resolveAssetPath(std::string_view relativePath)
    {
        std::vector<std::filesystem::path> roots;

        const std::filesystem::path exeDirectory = executableDirectory();
        if (!exeDirectory.empty())
        {
            roots.push_back(exeDirectory / "assets");
            roots.push_back(exeDirectory / ".." / "Resources" / "assets");
        }

#if defined(BS_SOURCE_ASSET_DIR)
        roots.emplace_back(BS_SOURCE_ASSET_DIR);
#endif

        std::string searched;

        for (const std::filesystem::path& root : roots)
        {
            const std::filesystem::path candidate = root / relativePath;

            std::error_code error;
            if (std::filesystem::is_regular_file(candidate, error))
            {
                return candidate;
            }

            searched += "\n  " + candidate.string();
        }

        throw std::runtime_error(
            "Asset not found: " + std::string(relativePath) + "\nSearched:" + searched
        );
    }
}
