#include <cstdint>
#include <memory>
#include <algorithm>
#include <print>
#include <string_view>
#include <system_error>
#include <ranges>
#include <vector>
#include <array>
#include <span>

#include <fstream>
#include <filesystem>

#include "glslang/Include/glslang_c_interface.h"
#include "glslang/Public/resource_limits_c.h"

import vkgc.cookbook_paths;


void compile_glsl_shader(
    std::string_view const shader_file_path,
    glslang_stage_t const shader_stage,
    char const* source_code,
    std::vector<uint32_t>& out_spirv_words)
{
    glslang_input_t const glslang_input{
        .language = GLSLANG_SOURCE_GLSL,
        .stage = shader_stage,

        .client = GLSLANG_CLIENT_VULKAN,
        .client_version = GLSLANG_TARGET_VULKAN_1_4,

        .target_language = GLSLANG_TARGET_SPV,
        .target_language_version = GLSLANG_TARGET_SPV_1_6,

        .code = source_code,

        .default_version = 100,
        .default_profile = GLSLANG_NO_PROFILE,
        .force_default_version_and_profile = false,
        .forward_compatible = false,
        .messages = GLSLANG_MSG_DEFAULT_BIT,
        .resource = glslang_default_resource(),
        .callbacks = {},
        .callbacks_ctx = nullptr
    };

    // Dismissible Scope Guard
    std::unique_ptr<glslang_shader_t, decltype(&glslang_shader_delete)> const shader{
        glslang_shader_create(&glslang_input),
        glslang_shader_delete};

    if (!shader)
    {
        std::println("GLSL shader creation failed {}", shader_file_path);
        return;
    }

    if (!glslang_shader_preprocess(shader.get(), &glslang_input))
    {
        std::println("GLSL preprocessing failed {}", shader_file_path);
        std::println("{}", glslang_shader_get_info_log(shader.get()));
        std::println("{}", glslang_shader_get_info_debug_log(shader.get()));
        std::println("{}", glslang_input.code);
        return;
    }

    if (!glslang_shader_parse(shader.get(), &glslang_input))
    {
        std::println("GLSL parsing failed {}", shader_file_path);
        std::println("{}", glslang_shader_get_info_log(shader.get()));
        std::println("{}", glslang_shader_get_info_debug_log(shader.get()));
        std::println("{}", glslang_shader_get_preprocessed_code(shader.get()));
        return;
    }

    // Dismissible Scope Guard
    std::unique_ptr<glslang_program_t, decltype(&glslang_program_delete)> const program{
        glslang_program_create(),
        glslang_program_delete};

    glslang_program_add_shader(program.get(), shader.get());

    if (!glslang_program_link(program.get(), GLSLANG_MSG_SPV_RULES_BIT | GLSLANG_MSG_VULKAN_RULES_BIT))
    {
        std::println("GLSL linking failed {}", shader_file_path);
        std::println("{}", glslang_program_get_info_log(program.get()));
        std::println("{}", glslang_program_get_info_debug_log(program.get()));
        return;
    }

    {
        glslang_spv_options_t options = {
            .generate_debug_info = true,
            .strip_debug_info = false,
            .disable_optimizer = false,
            .optimize_size = true,
            .disassemble = false,
            .validate = true,
            .emit_nonsemantic_shader_debug_info = false,
            .emit_nonsemantic_shader_debug_source = false,
            .compile_only = false,
            .optimize_allow_expanded_id_bound = false
        };
        glslang_program_SPIRV_generate_with_options(program.get(), shader_stage, &options);
    }

    // Number of words in SPIR-V binary
    auto const words_count = glslang_program_SPIRV_get_size(program.get());
    out_spirv_words.resize(words_count);

    glslang_program_SPIRV_get(program.get(), out_spirv_words.data());

    if (char const* spirv_messages = glslang_program_SPIRV_get_messages(program.get());
        spirv_messages != nullptr && spirv_messages[0] != '\0')
    {
        std::println("({}) {}", shader_file_path, spirv_messages);
    }

    std::println("[{}] is successfully done", shader_file_path);
}

std::vector<char> read_text_file(std::string_view const file_name)
{
    namespace fs = std::filesystem;

    fs::path const path = fs::path{vkgc::kShaderDir} / file_name;
    std::ifstream file{path, std::ios::in | std::ios::binary};

    if (file.fail())
    {
        return {};
    }

    auto const start_pos = file.tellg();

    file.ignore(std::numeric_limits<std::streamsize>::max());
    auto const chars_count = file.gcount();

    file.seekg(start_pos);

    if (chars_count < 1)
    {
        return {};
    }

    std::vector<std::ifstream::char_type> chars(static_cast<std::size_t>(chars_count) + 1, '\0');
    file.read(chars.data(), chars_count);

    if (file.fail())
    {
        return {};
    }

    if (constexpr std::array<std::ifstream::char_type, 3> utf8_bom{'\357', '\273', '\277'};
        std::ranges::equal(chars | std::views::take(3), utf8_bom))
    {
        std::fill_n(chars.begin(), 3, ' ');
    }

    return chars;
}

void save_spirv_byte_code(std::string_view const file_name, std::span<std::byte const> const byte_code)
{
    namespace fs = std::filesystem;

    fs::path const path = fs::path{vkgc::kCacheDir} / file_name;
    std::ofstream file{path, std::ios::binary};

    if (file.fail())
    {
        return;
    }

    file.write(
        reinterpret_cast<std::ostream::char_type const*>(byte_code.data()),
        static_cast<std::streamsize>(byte_code.size()));
}

void compile_and_save_shader(
    glslang_stage_t const shader_stage,
    std::string_view const src_file_path,
    std::string_view const dst_file_path)
{
    std::vector<char> const source_code = read_text_file(src_file_path);
    if (source_code.empty())
    {
        return;
    }

    std::vector<uint32_t> spirv_words; // uint32_t is SPIR-V word type
    compile_glsl_shader(src_file_path, shader_stage, source_code.data(), spirv_words);

    if (!spirv_words.empty())
    {
        save_spirv_byte_code(dst_file_path, std::as_bytes(std::span{spirv_words}));
    }
}

int main()
{
    if (!glslang_initialize_process())
    {
        return 1;
    }

    namespace fs = std::filesystem;
    if (std::error_code ec; !fs::create_directories(vkgc::kCacheDir, ec) && ec)
    {
        std::println("Failed to create cache directory {}: {}", vkgc::kCacheDir, ec.message());
        glslang_finalize_process();
        return 1;
    }

    compile_and_save_shader(
        glslang_stage_t::GLSLANG_STAGE_VERTEX,
        "chapter01/glslang/main.vert",
        "chapter01_glslang.vert.bin");

    compile_and_save_shader(
        glslang_stage_t::GLSLANG_STAGE_FRAGMENT,
        "chapter01/glslang/main.frag",
        "chapter01_glslang.frag.bin");

    glslang_finalize_process();
}
