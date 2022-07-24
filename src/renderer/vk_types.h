#pragma once

#include <vulkan/vulkan.h>>

struct Image{
    VkImage image;
    VkDeviceMemory memory;
};