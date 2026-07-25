#pragma once

#include <vulkan/vulkan.h>

namespace luu {

// The mandatory chain-plumbing hooks every layer must implement, plus the
// one this project actually cares about (QueuePresentKHR). Everything
// else is invisible to this layer - see vk_layer.cpp's GetInstanceProcAddr/
// GetDeviceProcAddr, which only intercept these names and delegate
// everything else down the chain.

VKAPI_ATTR VkResult VKAPI_CALL CreateInstance(const VkInstanceCreateInfo* pCreateInfo,
                                               const VkAllocationCallbacks* pAllocator,
                                               VkInstance* pInstance);
VKAPI_ATTR void VKAPI_CALL DestroyInstance(VkInstance instance,
                                            const VkAllocationCallbacks* pAllocator);

VKAPI_ATTR VkResult VKAPI_CALL CreateDevice(VkPhysicalDevice physicalDevice,
                                             const VkDeviceCreateInfo* pCreateInfo,
                                             const VkAllocationCallbacks* pAllocator,
                                             VkDevice* pDevice);
VKAPI_ATTR void VKAPI_CALL DestroyDevice(VkDevice device, const VkAllocationCallbacks* pAllocator);

// Skeleton: pure passthrough + a once-per-second stderr log. Drop-in point
// for real frame injection/upscaling later (same relationship
// upscale.frag/framegen.frag have to their future real implementations).
VKAPI_ATTR VkResult VKAPI_CALL QueuePresentKHR(VkQueue queue,
                                                const VkPresentInfoKHR* pPresentInfo);

// Looked up by dispatch key (see hooks.cpp) so GetInstanceProcAddr/
// GetDeviceProcAddr can delegate any non-hooked function name down the
// chain. Return nullptr if the handle has no known dispatch table entry.
PFN_vkGetInstanceProcAddr NextInstanceProcAddr(VkInstance instance);
PFN_vkGetDeviceProcAddr NextDeviceProcAddr(VkDevice device);

}  // namespace luu
