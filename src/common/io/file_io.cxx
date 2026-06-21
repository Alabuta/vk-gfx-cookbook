module;

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <ios>
#include <print>
#include <span>
#include <system_error>

#include "diagnostic/assert.hxx"

module vkgc.file_io;

namespace vkgc
{
    namespace file_io_detail
    {
        void check_size_is_multiple(
            [[maybe_unused]] std::size_t const byte_count,
            [[maybe_unused]] std::size_t const element_size)
        {
            VKGC_CHECK(byte_count % element_size == 0);
        }
    }

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
