#pragma once

#include <array>
#include <boost/thread/thread.hpp>
#include <memory>
#include <mutex>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>
#include <vulkan/vulkan_core.h>

struct SDL_Window;

namespace vulkan {

struct DeviceInfo {
  VkInstance       instance       = nullptr;
  VkPhysicalDevice physicalDevice = nullptr;
  VkDevice         device         = nullptr;
};

enum class QueueType : uint8_t {
  graphics = 0,
  compute,
  transfer,
  present,
  numTypes,
};

constexpr std::underlying_type<QueueType>::type getIndex(QueueType type) {
  return (std::underlying_type<QueueType>::type)type;
}

struct QueueInfo {
  VkQueue  queue    = nullptr;
  uint32_t family   = 0;
  size_t   useCount = 0;

  std::mutex mutex; // sync queue submit access

  QueueInfo(VkQueue queue_, uint32_t family_): queue(queue_), family(family_) {}
};

struct Queues {
  std::array<std::vector<std::unique_ptr<QueueInfo>>, getIndex(QueueType::numTypes)> items {}; /// first: VkQueue, second: familyindex
};

struct SwapchainData {
  VkSwapchainKHR swapchain = nullptr;
  VkFormat       format    = VK_FORMAT_UNDEFINED;
  VkExtent2D     extent2d  = {};

  // present sync
  uint64_t presentId = 0;
  uint64_t waitId    = 0;

  boost::mutex              mutexPresent;
  boost::condition_variable condPresent;

  // -

  struct DisplayBuffers {
    uint64_t bufferVaddr = 0;
    uint32_t bufferSize  = 0;
    uint32_t bufferAlign = 0;

    VkImage     image;
    VkSemaphore semDisplayReady;

    VkCommandBuffer transferBuffer;
    VkSemaphore     semPresentReady;

    VkFence bufferFence;
  };

  VkCommandPool               commandPool;
  std::vector<DisplayBuffers> buffers;
};

struct SurfaceCapabilities {
  VkSurfaceCapabilitiesKHR        capabilities {};
  std::vector<VkSurfaceFormatKHR> formats;
  std::vector<VkPresentModeKHR>   presentModes;

  bool format_srgb_bgra32  = false;
  bool format_unorm_bgra32 = false;
};

struct VulkanObj {
  DeviceInfo deviceInfo;

  VkDebugUtilsMessengerEXT debugMessenger = nullptr;
  SurfaceCapabilities      surfaceCapabilities;
  Queues                   queues;
};

VulkanObj* initVulkan(SDL_Window* window, VkSurfaceKHR& surface, bool useValidation);
void       deinitVulkan(VulkanObj* obj);

void createSurface(VulkanObj* obj, SDL_Window* window, VkSurfaceKHR& surfaceOut);

VkPhysicalDeviceLimits const* getPhysicalLimits();

VkInstance const getVkInstance();

std::string_view const getGPUName();

std::pair<VkFormat, VkColorSpaceKHR> getDisplayFormat(VulkanObj* obj);
void createData(VulkanObj* obj, VkSurfaceKHR surface, SwapchainData& swapchainData, uint32_t width, uint32_t height, bool enableVsync); // Swapchain
void destroySwapchain(VulkanObj* obj, SwapchainData& swapchainData);

} // namespace vulkan
