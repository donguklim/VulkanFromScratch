#ifndef WINDOWS_BUILD	// this is for letting the text editor to know windows build is defined 
#define WINDOWS_BUILD	
#endif 

#include <vulkan/vulkan.h>
#ifdef WINDOWS_BUILD
#include <vulkan/vulkan_win32.h>
#elif LINUX_BUILD
#endif
#include <iostream>
#include <iterator>

#define VK_CHECK(result)										\
	if(result != VK_SUCCESS) 									\
	{															\
		std::cout << "Vulkan Error: " << result << std::endl;	\
		__debugbreak();											\
		return false;											\
	}															\

static VKAPI_ATTR VkBool32 VKAPI_CALL vk_debug_callback(
	VkDebugUtilsMessageSeverityFlagBitsEXT msgSeverity,
	VkDebugUtilsMessageTypeFlagsEXT msgFlags,
	const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
	void* pUserData)
{
	std::cout << "Validation Error: " << pCallbackData->pMessage << std::endl;
	return false;
}

struct VkContext
{
	VkInstance instance;
	VkSurfaceKHR surface;
	VkPhysicalDevice gpu;
	VkDevice device;
	VkSwapchainKHR swapchain;
	VkDebugUtilsMessengerEXT debugMessenger;

	int graphicsIndex;
};

bool vk_init(VkContext* vkcontext,  void* window){
    VkApplicationInfo appInfo{};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = "Scratch";
	appInfo.pEngineName = "Scratchine";

	char* extensions[]{
#ifdef WINDOWS_BUILD
		VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
#elif LINUX_BUILD
#endif
		VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
		VK_KHR_SURFACE_EXTENSION_NAME
	};

	char* layers[]{
		"VK_LAYER_KHRONOS_validation"
	};

	VkInstanceCreateInfo instanceInfo{};
	instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	instanceInfo.pApplicationInfo = &appInfo;
	instanceInfo.ppEnabledExtensionNames = extensions;
	instanceInfo.enabledExtensionCount = std::size(extensions);
	instanceInfo.ppEnabledLayerNames = layers;
	instanceInfo.enabledLayerCount = std::size(layers);
	VK_CHECK(vkCreateInstance(&instanceInfo, nullptr, &vkcontext->instance));

	auto vkCreateDebugUtilsMessengerEXT = (PFN_vkCreateDebugUtilsMessengerEXT)(vkGetInstanceProcAddr(vkcontext->instance, "vkCreateDebugUtilsMessengerEXT"));


	if(vkCreateDebugUtilsMessengerEXT)
	{
		VkDebugUtilsMessengerCreateInfoEXT debugInfo{};
		debugInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
		debugInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT;
		debugInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
		debugInfo.pfnUserCallback = vk_debug_callback;
		vkCreateDebugUtilsMessengerEXT(vkcontext->instance, &debugInfo, nullptr, &vkcontext->debugMessenger);
	}
	else {
		return false;
	}

	// Create Surface
	{
#ifdef WINDOWS_BUILD
		VkWin32SurfaceCreateInfoKHR surfaceInfo{};
		surfaceInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
		surfaceInfo.hwnd = static_cast<HWND>(window);
		surfaceInfo.hinstance = GetModuleHandle(nullptr);
		VK_CHECK(vkCreateWin32SurfaceKHR(vkcontext->instance, &surfaceInfo, nullptr, &vkcontext->surface));
#elif LINUX_BUILD
#endif
	}

	// Choose GPU
	{
		vkcontext->graphicsIndex = -1;
		uint32_t gpuCount{0};
		VkPhysicalDevice gpus[10];
		VK_CHECK(vkEnumeratePhysicalDevices(vkcontext->instance, &gpuCount, nullptr));
		VK_CHECK(vkEnumeratePhysicalDevices(vkcontext->instance, &gpuCount, gpus));

		for (uint32_t i=0; i < gpuCount; i++){
			VkPhysicalDevice gpu = gpus[i];
			uint32_t queueFamilyCount = 0;

			VkQueueFamilyProperties queueProps[10];
			vkGetPhysicalDeviceQueueFamilyProperties(gpu, &queueFamilyCount, nullptr);
			vkGetPhysicalDeviceQueueFamilyProperties(gpu, &queueFamilyCount, queueProps);

			for(uint32_t j=0; j < queueFamilyCount; j++)
			{
				if(queueProps[j].queueFlags & VK_QUEUE_GRAPHICS_BIT)
				{
					VkBool32 surfaceSupport = VK_FALSE;
					VK_CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(gpu, j, vkcontext->surface, &surfaceSupport));

					if (surfaceSupport)
					{
						vkcontext->graphicsIndex = j;
						vkcontext->gpu = gpu;
						break;
					}

				}
			}
		}

		if (vkcontext->graphicsIndex < 0)
		{
			return false;
		}
	}

	// Logical Device
	{
		float queuePriority = 1.0f;
		VkDeviceQueueCreateInfo queueInfo{};
		queueInfo.sType= VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueInfo.queueFamilyIndex = vkcontext->graphicsIndex;
		queueInfo.queueCount = 1;
		queueInfo.pQueuePriorities = &queuePriority;

		char* extensions[] ={
			VK_KHR_SWAPCHAIN_EXTENSION_NAME
		};
		
		VkDeviceCreateInfo deviceInfo{};
		deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		deviceInfo.pQueueCreateInfos = &queueInfo;
		deviceInfo.queueCreateInfoCount = 1;
		deviceInfo.ppEnabledExtensionNames = extensions;
		deviceInfo.enabledExtensionCount = std::size(extensions);
	
		VK_CHECK(vkCreateDevice(vkcontext->gpu, &deviceInfo, nullptr, &vkcontext->device));
	}

	// Swapchain
	{
		VkSurfaceCapabilitiesKHR surfaceCaps{};
		VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vkcontext->gpu, vkcontext->surface, &surfaceCaps));
		uint32_t imgCount = surfaceCaps.minImageCount + 1;
		imgCount = imgCount > surfaceCaps.maxImageCount ? imgCount - 1 : imgCount;

		VkSwapchainCreateInfoKHR scInfo{};
		scInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		scInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		scInfo.surface = vkcontext->surface;
		scInfo.preTransform = surfaceCaps.currentTransform;
		scInfo.imageExtent = surfaceCaps.currentExtent;
		scInfo.minImageCount = imgCount;
		
		VK_CHECK(vkCreateSwapchainKHR(vkcontext->device, &scInfo, nullptr, &vkcontext->swapchain));
	}

	return true;
}