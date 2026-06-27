module;

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <print>
#include <span>
#include <system_error>
#include <vector>

#include "diagnostic/assert.hxx"

module vkgc.file_io;

namespace vkgc
{
    template <typename T>
    std::vector<T> load_binary_file(std::filesystem::path const& path)
    {
        std::ifstream file{path, std::ios::in | std::ios::binary | std::ios::ate};

        if (file.fail())
        {
            return {};
        }

        std::streamsize const chars_count = file.tellg();
        if (chars_count < 1)
        {
            return {};
        }

        file.seekg(0, std::ios::beg);

        VKGC_CHECK(static_cast<std::size_t>(chars_count) % sizeof(T) == 0);

        std::vector<T> bytes(static_cast<std::size_t>(chars_count) / sizeof(T));
        file.read(reinterpret_cast<std::istream::char_type*>(bytes.data()), chars_count);

        if (file.fail())
        {
            return {};
        }

        return bytes;
    }

    template std::vector<std::uint32_t> load_binary_file<std::uint32_t>(std::filesystem::path const&);
    template std::vector<std::byte> load_binary_file<std::byte>(std::filesystem::path const&);

    bool save_binary_file(std::filesystem::path const& path, std::span<std::byte const> const bytes)
    {
        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);

        if (ec)
        {
            std::println(stderr, "[FileIO] : Error : failed to save file [{}] : {}", ec.value(), ec.message());
            return false;
        }

        std::ofstream file{path, std::ios::out | std::ios::binary | std::ios::trunc};
        if (file.fail())
        {
            return false;
        }

        file.write(reinterpret_cast<char const*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));

        return !file.fail();
    }
}
