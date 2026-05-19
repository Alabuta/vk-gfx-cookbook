function(configure_cookbook_target TARGET_NAME)

    # Compiler/platform dispatch via generator expressions (evaluated per-target at generation time).
    # Using $<CXX_COMPILER_ID:...> rather than $<COMPILE_LANG_AND_ID:CXX,...> because the latter is
    # only valid for compile properties; CXX_COMPILER_ID works in link-library and link-option contexts too.
    set(IS_GNU_LINUX "$<AND:$<CXX_COMPILER_ID:GNU>,$<NOT:$<PLATFORM_ID:Windows>>>")
    set(IS_MINGW     "$<AND:$<CXX_COMPILER_ID:GNU>,$<PLATFORM_ID:Windows>>")
    set(IS_CLANG_CL  "$<AND:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_FRONTEND_VARIANT:MSVC>>")
    set(IS_MSVC      "$<CXX_COMPILER_ID:MSVC>")

    target_include_directories(${TARGET_NAME}
        PRIVATE
            ${CMAKE_SOURCE_DIR}/src
    )

    target_sources(${TARGET_NAME}
        PRIVATE
            ${CMAKE_SOURCE_DIR}/src/common/app/bootstrap.cxx
            ${CMAKE_SOURCE_DIR}/src/common/app/window.cxx
            ${CMAKE_SOURCE_DIR}/src/common/vulkan/instance.cxx
            ${CMAKE_SOURCE_DIR}/src/common/vulkan/instance_device_factory.cxx
            ${CMAKE_SOURCE_DIR}/src/common/vulkan/surface.cxx
            ${CMAKE_SOURCE_DIR}/src/common/vulkan/device.cxx

        PRIVATE
            FILE_SET cookbook_modules TYPE CXX_MODULES
                BASE_DIRS ${CMAKE_SOURCE_DIR}/src/common
                FILES
                    ${CMAKE_SOURCE_DIR}/src/common/app/bootstrap.cxxm
                    ${CMAKE_SOURCE_DIR}/src/common/app/window.cxxm
                    ${CMAKE_SOURCE_DIR}/src/common/vulkan/helpers.cxxm
                    ${CMAKE_SOURCE_DIR}/src/common/vulkan/instance.cxxm
                    ${CMAKE_SOURCE_DIR}/src/common/vulkan/surface.cxxm
                    ${CMAKE_SOURCE_DIR}/src/common/vulkan/device.cxxm
    )

    target_link_libraries(${TARGET_NAME}
        PRIVATE
            "$<$<OR:${IS_GNU_LINUX},${IS_MINGW}>:"
                stdc++fs
                stdc++exp
            ">"

            #Vulkan::Headers
            volk::volk

            GPUOpen::VulkanMemoryAllocator

            glm::glm
            glfw
            Taskflow::Taskflow

            SPIRV
            glslang
            glslang-default-resource-limits
    )

    set_target_properties(${TARGET_NAME}
        PROPERTIES
            CXX_STANDARD 23
            CXX_STANDARD_REQUIRED ON
            CXX_EXTENSIONS OFF

            POSITION_INDEPENDENT_CODE ON

            DEBUG_POSTFIX .d

            XCODE_GENERATE_SCHEME          TRUE
            XCODE_SCHEME_WORKING_DIRECTORY "$<$<PLATFORM_ID:Darwin>:${CMAKE_SOURCE_DIR}>"
            VS_DEBUGGER_WORKING_DIRECTORY  "$<$<PLATFORM_ID:Windows>:${CMAKE_SOURCE_DIR}>"
    )

    target_compile_definitions(${TARGET_NAME}
        PRIVATE
            $<$<CONFIG:Debug>:VULKAN_DEBUG>

            # To avoid symbolic conflicts between 'volk.h' and 'vulkan/vulkan.h'
            VK_NO_PROTOTYPES

            # VMA configuration macros
            VMA_STATIC_VULKAN_FUNCTIONS=0
            VMA_DYNAMIC_VULKAN_FUNCTIONS=1

            # Makes the GLFW header not include any OpenGL or OpenGL ES API header
            GLFW_INCLUDE_NONE

            # volk and GLFW3-native required platform specific defines
            "$<$<PLATFORM_ID:Linux>:"
                VK_USE_PLATFORM_WAYLAND_KHR
            ">"

            "$<$<PLATFORM_ID:Darwin>:"
                VK_USE_PLATFORM_MACOS_MVK
            ">"

            "$<$<PLATFORM_ID:Windows>:"
                VK_USE_PLATFORM_WIN32_KHR

                GLFW_EXPOSE_NATIVE_WIN32
                GLFW_NATIVE_INCLUDE_NONE
            ">"

            COOKBOOK_SHADER_DIR_STRING="${CMAKE_SOURCE_DIR}/shaders/"
            COOKBOOK_CACHE_DIR_STRING="${CMAKE_SOURCE_DIR}/.cache/"
            COOKBOOK_LOADER_SETTINGS_FILE_STRING="${CMAKE_SOURCE_DIR}/vk_loader_settings.json"
    )

    target_compile_options(${TARGET_NAME}
        PRIVATE

            "$<$<OR:${IS_GNU_LINUX},${IS_CLANG_CL},${IS_MINGW}>:"
                -Wpedantic
                -Wall
                -Wextra
                -Werror
                -Wconversion

                -Wold-style-cast
                -Wnon-virtual-dtor
                -Wcast-align
                -Wunused
                -Woverloaded-virtual
                -Wsign-conversion
                -Wnull-dereference
                -Wdouble-promotion
                -Wformat=2
                -Wmisleading-indentation

                -Wno-c++98-compat
                -Wno-c++98-compat-pedantic

                -Wno-pre-c++17-compat
            ">"

            "$<$<OR:${IS_GNU_LINUX},${IS_MINGW}>:"
                -fasynchronous-unwind-tables                # Increased reliability of backtraces
                -fexceptions                                # Enable table-based thread cancellation

                -pipe

                -Wduplicated-cond
                -Wduplicated-branches
                -Wlogical-op
                -Wuseless-cast
            ">"

            "$<${IS_CLANG_CL}:"
                /EHa

                -Wno-unknown-pragmas
                -Wno-unknown-warning-option

                -Wno-shadow-field-in-constructor
            ">"

            "$<${IS_MSVC}:"
                # /W4
                # /WX
                /w14242 # 'identifier': conversion from 'type1' to 'type1', possible loss of data
                /w14254 # 'operator': conversion from 'type1:field_bits' to 'type2:field_bits', possible loss of data
                /w14263 # 'function': member function does not override any base class virtual member function
                /w14265 # 'classname': class has virtual functions, but destructor is not virtual
                /w14287 # 'operator': unsigned/negative constant mismatch
                /we4289 # 'variable': loop control variable declared in the for-loop is used outside the for-loop scope
                /w14296 # 'operator': expression is always 'boolean_value'
                /w14311 # 'variable': pointer truncation from 'type1' to 'type2'
                /w14545 # expression before comma evaluates to a function which is missing an argument list
                /w14546 # function call before comma missing argument list
                /w14547 # 'operator': operator before comma has no effect; expected operator with side-effect
                /w14549 # 'operator': operator before comma has no effect; did you intend 'operator'?
                /w14555 # expression has no effect; expected expression with side-effect
                /w14619 # pragma warning: there is no warning number 'number'
                /w14640 # Enable warning on thread un-safe static member initialization
                /w14826 # Conversion from 'type1' to 'type_2' is sign-extended. This may cause unexpected runtime behavior.
                /w14905 # wide string literal cast to 'LPSTR'
                /w14906 # string literal cast to 'LPWSTR'
                /w14928 # illegal copy-initialization; more than one user-defined conversion has been implicitly applied
            ">"
    )

    target_link_options(${TARGET_NAME}
        PRIVATE
            "$<$<OR:$<PLATFORM_ID:Linux>,$<PLATFORM_ID:Darwin>>:"
                LINKER:-z,defs;                         # Detect and reject underlinking
                LINKER:-z,now;                          # Disable lazy binding
                LINKER:-z,relro                         # Read-only segments after relocation
            ">"

            "$<$<OR:${IS_GNU_LINUX},${IS_MINGW}>:"
                LINKER:-no-undefined;                   # Report unresolved symbol references from regular object files
                LINKER:-no-allow-shlib-undefined;       # Disallows undefined symbols in shared libraries
                LINKER:-unresolved-symbols=report-all
            ">"
    )

endfunction()
