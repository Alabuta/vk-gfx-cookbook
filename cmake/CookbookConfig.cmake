function(configure_cookbook_target TARGET_NAME)

    if(MSVC)
        target_compile_definitions(${TARGET_NAME} PRIVATE _CONSOLE)
    endif()

    set_target_properties(${TARGET_NAME}
        PROPERTIES
            CXX_STANDARD 26
            CXX_STANDARD_REQUIRED ON
            CXX_EXTENSIONS OFF

            POSITION_INDEPENDENT_CODE ON

            DEBUG_POSTFIX .d
    )

    if(APPLE)
        set_target_properties(${TARGET_NAME}
            PROPERTIES
                XCODE_GENERATE_SCHEME TRUE
                XCODE_SCHEME_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        )
    endif()

    if(MSVC)
        set_target_properties(${TARGET_NAME}
            PROPERTIES
                VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        )
    endif()

    target_compile_options(${TARGET_NAME}
        PRIVATE

            "$<$<OR:$<BOOL:${CXX_FLAGS_STYLE_GNU}>,$<BOOL:${CXX_FLAGS_STYLE_CLANGCL}>,$<BOOL:${CXX_FLAGS_STYLE_MINGW_WINDOWS}>>:"
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

            "$<$<OR:$<BOOL:${CXX_FLAGS_STYLE_GNU}>,$<BOOL:${CXX_FLAGS_STYLE_MINGW_WINDOWS}>>:"
                -fconcepts

                -fasynchronous-unwind-tables                # Increased reliability of backtraces
                -fexceptions                                # Enable table-based thread cancellation

                -pipe

                -Wduplicated-cond
                -Wduplicated-branches
                -Wlogical-op
                -Wuseless-cast
            ">"

            "$<$<BOOL:${CXX_FLAGS_STYLE_CLANGCL}>:"
                /EHa

                -Wno-unknown-pragmas
                -Wno-unknown-warning-option

                -Wno-shadow-field-in-constructor
            ">"

            "$<$<BOOL:${CXX_FLAGS_STYLE_MSVC}>:"
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

    target_link_libraries(${TARGET_NAME}
        PRIVATE
            "$<$<OR:$<BOOL:${CXX_FLAGS_STYLE_GNU}>,$<BOOL:${CXX_FLAGS_STYLE_MINGW_WINDOWS}>>:"
                stdc++fs
                stdc++exp
            ">"

            glm::glm
            glfw
            Taskflow::Taskflow
    )

    target_link_options(${TARGET_NAME}
        PRIVATE
            "$<$<IN_LIST:${CMAKE_SYSTEM_NAME},Linux;Darwin>:"
                LINKER:-z,defs;                         # Detect and reject underlinking
                LINKER:-z,now;                          # Disable lazy binding
                LINKER:-z,relro                         # Read-only segments after relocation
            ">"

            "$<$<OR:$<BOOL:${CXX_FLAGS_STYLE_GNU}>,$<BOOL:${CXX_FLAGS_STYLE_MINGW_WINDOWS}>>:"
                LINKER:-no-undefined;                   # Report unresolved symbol references from regular object files
                LINKER:-no-allow-shlib-undefined;       # Disallows undefined symbols in shared libraries
                LINKER:-unresolved-symbols=report-all
            ">"
    )

endfunction()
