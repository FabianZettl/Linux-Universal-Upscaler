#pragma once

#include <vulkan/vulkan.h>

namespace luu {

// Intercepts only the names hooks.h declares; everything else is
// delegated to the next link in the chain (see hooks.cpp's
// Next{Instance,Device}ProcAddr). Handed to the loader via
// vkNegotiateLoaderLayerInterfaceVersion (vk_layer.cpp) - the loader
// interface v2 way of registering a layer's proc-addr functions, no
// special exported-symbol-name convention needed for these two.
VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL GetInstanceProcAddr(VkInstance instance, const char* pName);
VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL GetDeviceProcAddr(VkDevice device, const char* pName);

}  // namespace luu
