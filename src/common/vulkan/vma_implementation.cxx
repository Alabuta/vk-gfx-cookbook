// Single non-module TU that materializes the Vulkan Memory Allocator
// definitions. Kept out of any `module X;` purview because Clang's module
// machinery mis-resolves the in-header `static` `VmaFree` overloads when
// VMA_IMPLEMENTATION is expanded in a module unit that also `import`s
// another module whose GMF `#include`d vk_mem_alloc.h.

#include <volk.h>

#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"
