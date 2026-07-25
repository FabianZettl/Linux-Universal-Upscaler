#include "hooks.h"

#include <vulkan/vk_layer.h>

#include <atomic>
#include <chrono>
#include <iostream>
#include <mutex>
#include <unordered_map>

namespace luu {

namespace {

struct InstanceDispatch {
    PFN_vkGetInstanceProcAddr GetInstanceProcAddr = nullptr;  // "next" in the chain
    PFN_vkDestroyInstance DestroyInstance = nullptr;
};

struct DeviceDispatch {
    PFN_vkGetDeviceProcAddr GetDeviceProcAddr = nullptr;  // "next" in the chain
    PFN_vkDestroyDevice DestroyDevice = nullptr;
    PFN_vkQueuePresentKHR QueuePresentKHR = nullptr;
};

std::mutex g_mutex;
std::unordered_map<void*, InstanceDispatch> g_instanceTables;
std::unordered_map<void*, DeviceDispatch> g_deviceTables;

// Vulkan dispatchable handles (VkInstance, VkDevice, VkQueue, ...) are
// pointers whose pointee's first word is the loader's dispatch pointer -
// the standard way layers key per-object dispatch tables. A VkQueue
// shares its parent VkDevice's dispatch key, which is why QueuePresentKHR
// can look itself up in g_deviceTables directly.
template <typename Handle>
void* dispatchKey(Handle handle) {
    return *reinterpret_cast<void* const*>(handle);
}

}  // namespace

VKAPI_ATTR VkResult VKAPI_CALL CreateInstance(const VkInstanceCreateInfo* pCreateInfo,
                                               const VkAllocationCallbacks* pAllocator,
                                               VkInstance* pInstance) {
    auto* layerCreateInfo = const_cast<VkLayerInstanceCreateInfo*>(
        reinterpret_cast<const VkLayerInstanceCreateInfo*>(pCreateInfo->pNext));
    while (layerCreateInfo && !(layerCreateInfo->sType == VK_STRUCTURE_TYPE_LOADER_INSTANCE_CREATE_INFO &&
                                 layerCreateInfo->function == VK_LAYER_LINK_INFO)) {
        layerCreateInfo = const_cast<VkLayerInstanceCreateInfo*>(
            reinterpret_cast<const VkLayerInstanceCreateInfo*>(layerCreateInfo->pNext));
    }
    if (!layerCreateInfo) {
        std::cerr << "[luu-vklayer] Error: no VK_LAYER_LINK_INFO in vkCreateInstance chain\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    PFN_vkGetInstanceProcAddr nextGetInstanceProcAddr =
        layerCreateInfo->u.pLayerInfo->pfnNextGetInstanceProcAddr;
    // Advance the link before calling down so the next layer sees itself first.
    layerCreateInfo->u.pLayerInfo = layerCreateInfo->u.pLayerInfo->pNext;

    auto fpCreateInstance = reinterpret_cast<PFN_vkCreateInstance>(
        nextGetInstanceProcAddr(VK_NULL_HANDLE, "vkCreateInstance"));
    if (!fpCreateInstance) return VK_ERROR_INITIALIZATION_FAILED;

    VkResult result = fpCreateInstance(pCreateInfo, pAllocator, pInstance);
    if (result != VK_SUCCESS) return result;

    InstanceDispatch table;
    table.GetInstanceProcAddr = nextGetInstanceProcAddr;
    table.DestroyInstance = reinterpret_cast<PFN_vkDestroyInstance>(
        nextGetInstanceProcAddr(*pInstance, "vkDestroyInstance"));

    std::lock_guard<std::mutex> lock(g_mutex);
    g_instanceTables[dispatchKey(*pInstance)] = table;
    return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL DestroyInstance(VkInstance instance,
                                            const VkAllocationCallbacks* pAllocator) {
    PFN_vkDestroyInstance destroyInstance = nullptr;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        auto it = g_instanceTables.find(dispatchKey(instance));
        if (it != g_instanceTables.end()) {
            destroyInstance = it->second.DestroyInstance;
            g_instanceTables.erase(it);
        }
    }
    if (destroyInstance) destroyInstance(instance, pAllocator);
}

VKAPI_ATTR VkResult VKAPI_CALL CreateDevice(VkPhysicalDevice physicalDevice,
                                             const VkDeviceCreateInfo* pCreateInfo,
                                             const VkAllocationCallbacks* pAllocator,
                                             VkDevice* pDevice) {
    auto* layerCreateInfo = const_cast<VkLayerDeviceCreateInfo*>(
        reinterpret_cast<const VkLayerDeviceCreateInfo*>(pCreateInfo->pNext));
    while (layerCreateInfo && !(layerCreateInfo->sType == VK_STRUCTURE_TYPE_LOADER_DEVICE_CREATE_INFO &&
                                 layerCreateInfo->function == VK_LAYER_LINK_INFO)) {
        layerCreateInfo = const_cast<VkLayerDeviceCreateInfo*>(
            reinterpret_cast<const VkLayerDeviceCreateInfo*>(layerCreateInfo->pNext));
    }
    if (!layerCreateInfo) {
        std::cerr << "[luu-vklayer] Error: no VK_LAYER_LINK_INFO in vkCreateDevice chain\n";
        return VK_ERROR_INITIALIZATION_FAILED;
    }

    PFN_vkGetInstanceProcAddr nextGetInstanceProcAddr =
        layerCreateInfo->u.pLayerInfo->pfnNextGetInstanceProcAddr;
    PFN_vkGetDeviceProcAddr nextGetDeviceProcAddr =
        layerCreateInfo->u.pLayerInfo->pfnNextGetDeviceProcAddr;
    layerCreateInfo->u.pLayerInfo = layerCreateInfo->u.pLayerInfo->pNext;

    auto fpCreateDevice = reinterpret_cast<PFN_vkCreateDevice>(
        nextGetInstanceProcAddr(VK_NULL_HANDLE, "vkCreateDevice"));
    if (!fpCreateDevice) return VK_ERROR_INITIALIZATION_FAILED;

    VkResult result = fpCreateDevice(physicalDevice, pCreateInfo, pAllocator, pDevice);
    if (result != VK_SUCCESS) return result;

    DeviceDispatch table;
    table.GetDeviceProcAddr = nextGetDeviceProcAddr;
    table.DestroyDevice =
        reinterpret_cast<PFN_vkDestroyDevice>(nextGetDeviceProcAddr(*pDevice, "vkDestroyDevice"));
    table.QueuePresentKHR = reinterpret_cast<PFN_vkQueuePresentKHR>(
        nextGetDeviceProcAddr(*pDevice, "vkQueuePresentKHR"));

    std::lock_guard<std::mutex> lock(g_mutex);
    g_deviceTables[dispatchKey(*pDevice)] = table;
    return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL DestroyDevice(VkDevice device, const VkAllocationCallbacks* pAllocator) {
    PFN_vkDestroyDevice destroyDevice = nullptr;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        auto it = g_deviceTables.find(dispatchKey(device));
        if (it != g_deviceTables.end()) {
            destroyDevice = it->second.DestroyDevice;
            g_deviceTables.erase(it);
        }
    }
    if (destroyDevice) destroyDevice(device, pAllocator);
}

VKAPI_ATTR VkResult VKAPI_CALL QueuePresentKHR(VkQueue queue, const VkPresentInfoKHR* pPresentInfo) {
    PFN_vkQueuePresentKHR next = nullptr;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        auto it = g_deviceTables.find(dispatchKey(queue));
        if (it != g_deviceTables.end()) next = it->second.QueuePresentKHR;
    }
    if (!next) return VK_ERROR_DEVICE_LOST;  // shouldn't happen if CreateDevice succeeded

    static std::atomic<uint64_t> presentCount{0};
    static auto windowStart = std::chrono::steady_clock::now();
    ++presentCount;
    auto now = std::chrono::steady_clock::now();
    if (now - windowStart >= std::chrono::seconds(1)) {
        std::cerr << "[luu-vklayer] " << presentCount.load() << " presents/sec (passthrough)\n";
        presentCount = 0;
        windowStart = now;
    }

    return next(queue, pPresentInfo);
}

PFN_vkGetInstanceProcAddr NextInstanceProcAddr(VkInstance instance) {
    if (!instance) return nullptr;
    std::lock_guard<std::mutex> lock(g_mutex);
    auto it = g_instanceTables.find(dispatchKey(instance));
    return it != g_instanceTables.end() ? it->second.GetInstanceProcAddr : nullptr;
}

PFN_vkGetDeviceProcAddr NextDeviceProcAddr(VkDevice device) {
    if (!device) return nullptr;
    std::lock_guard<std::mutex> lock(g_mutex);
    auto it = g_deviceTables.find(dispatchKey(device));
    return it != g_deviceTables.end() ? it->second.GetDeviceProcAddr : nullptr;
}

}  // namespace luu
