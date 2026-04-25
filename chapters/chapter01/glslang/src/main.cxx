#include <cstdint>
#include <memory>
#include <print>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>
#include <array>
#include <span>

#include <fstream>
#include <filesystem>

#include "glslang/Include/glslang_c_interface.h"
#include "glslang/Public/resource_limits_c.h"

constexpr std::string_view kCOOKBOOK_SHADER_DIR_STRING = COOKBOOK_SHADER_DIR_STRING;
constexpr std::string_view kCOOKBOOK_CACHE_DIR_STRING = COOKBOOK_CACHE_DIR_STRING;


void compile_glsl_shader(
    const std::string_view shader_file_path,
    const glslang_stage_t shader_stage,
    const char* source_code,
    std::vector<uint32_t>& out_spirv_words)
{
    const glslang_input_t glslang_input{
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
    const std::unique_ptr<glslang_shader_t, decltype(&glslang_shader_delete)> shader{
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
    const std::unique_ptr<glslang_program_t, decltype(&glslang_program_delete)> program{
        glslang_program_create(),
        glslang_program_delete
    };

    glslang_program_add_shader(program.get(), shader.get());

    if (!glslang_program_link(program.get(), GLSLANG_MSG_SPV_RULES_BIT | GLSLANG_MSG_VULKAN_RULES_BIT))
    {
        std::println("GLSL linking failed {}", shader_file_path);
        std::println("{}", glslang_shader_get_info_log(shader.get()));
        std::println("{}", glslang_shader_get_info_debug_log(shader.get()));
        return;
    }

    glslang_program_SPIRV_generate(program.get(), shader_stage);

    // Number of words in SPIR-V binary
    auto const words_count = glslang_program_SPIRV_get_size(program.get());
    out_spirv_words.resize(words_count);

    glslang_program_SPIRV_get(program.get(), out_spirv_words.data());

    if (const char* spirv_messages = glslang_program_SPIRV_get_messages(program.get());
        spirv_messages != nullptr && spirv_messages[0] != '\0')
    {
        std::println("({}) {}", shader_file_path, spirv_messages);
    }

    std::println("[{}] is successfully done", shader_file_path);
}

std::vector<char> read_text_file(const std::string_view file_name)
{
    namespace fs = std::filesystem;

    const fs::path path = fs::path{kCOOKBOOK_SHADER_DIR_STRING} / file_name;
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
    file.read(std::data(chars), chars_count);

    if (file.fail())
    {
        return {};
    }

    if (constexpr std::array<std::ifstream::char_type, 3> utf8_bom{'\357', '\273', '\277'};
        std::equal(std::begin(utf8_bom), std::end(utf8_bom), std::begin(chars)))
    {
        std::fill_n(std::begin(chars), 3, ' ');
    }

    return chars;
}

void save_spirv_byte_code(const std::string_view file_name, const std::span<const std::byte> byte_code)
{
    namespace fs = std::filesystem;

    const fs::path path = fs::path{kCOOKBOOK_CACHE_DIR_STRING} / file_name;
    std::ofstream file{path, std::ios::binary};

    if (file.fail())
    {
        return;
    }

    file.write(
        reinterpret_cast<const std::ostream::char_type*>(std::data(byte_code)),
        static_cast<std::streamsize>(std::size(byte_code)));
}

void compile_and_save_shader(
    const glslang_stage_t shader_stage,
    const std::string_view src_file_path,
    const std::string_view dst_file_path)
{
    const std::vector<char> source_code = read_text_file(src_file_path);
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
    if (std::error_code ec; !fs::create_directories(kCOOKBOOK_CACHE_DIR_STRING, ec) && ec)
    {
        std::println("Failed to create cache directory {}: {}", kCOOKBOOK_CACHE_DIR_STRING, ec.message());
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
