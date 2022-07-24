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

#include "platform.h"
#include "vk_init.cpp"
#include "dds_structs.h"

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
	VkExtent2D screenSize;

	VkInstance instance;
	VkDebugUtilsMessengerEXT debugMessenger;
	VkSurfaceKHR surface;
	VkSurfaceFormatKHR surfaceFormat;

	VkPhysicalDevice gpu;
	VkDevice device;
	VkQueue graphicsQueue;

	VkSwapchainKHR swapchain;
	VkRenderPass renderpass;
	VkCommandPool commandPool;

	VkPipelineLayout pipeLayout;
	VkPipeline pipeline;
	VkImage image;


	VkSemaphore submitSemaphore;
	VkSemaphore acquireSemaphore;

	uint32_t scImgCount;
	VkImage scImages[5];
	VkImageView scImageViews[5];
	VkFramebuffer framebuffers[5];

	int graphicsIndex;
};

bool vk_init(VkContext* vkcontext,  void* window)
{
	platform_get_window_size(&vkcontext->screenSize.width, &vkcontext->screenSize.height);
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
	else 
	{
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

		vkGetDeviceQueue(vkcontext->device, vkcontext->graphicsIndex, 0, &vkcontext->graphicsQueue);
	}

	// Swapchain
	{
		uint32_t formatCount = 0;
		VkSurfaceFormatKHR surfaceFormats[10];
		VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(vkcontext->gpu, vkcontext->surface, &formatCount, nullptr));
		VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(vkcontext->gpu, vkcontext->surface, &formatCount, surfaceFormats));

		for (uint32_t i = 0; i < formatCount; i++)
		{
			VkSurfaceFormatKHR format = surfaceFormats[i];

			if(format.format == VK_FORMAT_B8G8R8A8_SRGB)
			{
				vkcontext->surfaceFormat = format;
				break;
			}
		}


		VkSurfaceCapabilitiesKHR surfaceCaps{};
		VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vkcontext->gpu, vkcontext->surface, &surfaceCaps));
		uint32_t imgCount = surfaceCaps.minImageCount + 1;
		imgCount = imgCount > surfaceCaps.maxImageCount ? imgCount - 1 : imgCount;

		VkSwapchainCreateInfoKHR scInfo{};
		scInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		scInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		scInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		scInfo.surface = vkcontext->surface;
		scInfo.imageFormat = vkcontext->surfaceFormat.format;
		scInfo.preTransform = surfaceCaps.currentTransform;
		scInfo.imageExtent = surfaceCaps.currentExtent;
		scInfo.minImageCount = imgCount;
		scInfo.imageArrayLayers = 1;
		
		VK_CHECK(vkCreateSwapchainKHR(vkcontext->device, &scInfo, nullptr, &vkcontext->swapchain));
		VK_CHECK(vkGetSwapchainImagesKHR(vkcontext->device, vkcontext->swapchain, &vkcontext->scImgCount, nullptr));
		VK_CHECK(vkGetSwapchainImagesKHR(vkcontext->device, vkcontext->swapchain, &vkcontext->scImgCount, vkcontext->scImages));

		// Create the image views
		{
			VkImageViewCreateInfo viewInfo{};
			viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			viewInfo.format = vkcontext->surfaceFormat.format;
			viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			viewInfo.subresourceRange.layerCount = 1;
			viewInfo.subresourceRange.levelCount = 1;
	
			for(uint32_t i = 0; i < vkcontext->scImgCount; i++)
			{
				viewInfo.image = vkcontext->scImages[i];
				VK_CHECK(vkCreateImageView(vkcontext->device, &viewInfo, nullptr, &vkcontext->scImageViews[i]));
			}
		}
	}

	// Renderpass
	{
		VkAttachmentDescription attachment{};
		attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachment.samples = VK_SAMPLE_COUNT_1_BIT;
		attachment.format = vkcontext->surfaceFormat.format;

		VkAttachmentReference colorAttachmentRef{};
		colorAttachmentRef.attachment = 0;
		colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkSubpassDescription subpassDesc{};
		subpassDesc.colorAttachmentCount = 1;
		subpassDesc.pColorAttachments = &colorAttachmentRef;

		VkAttachmentDescription attachments[] = { attachment };

		VkRenderPassCreateInfo rpInfo{};
		rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		rpInfo.pAttachments = attachments;
		rpInfo.attachmentCount = std::size(attachments);
		rpInfo.subpassCount = 1;
		rpInfo.pSubpasses = &subpassDesc;
		
		VK_CHECK(vkCreateRenderPass(vkcontext->device, &rpInfo, nullptr, &vkcontext->renderpass));
	}

	// Framebuffers
	{
		VkFramebufferCreateInfo fbInfo{};
		fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		fbInfo.width = vkcontext->screenSize.width;
		fbInfo.height = vkcontext->screenSize.height;
		fbInfo.renderPass = vkcontext->renderpass;
		fbInfo.layers = 1;
		fbInfo.attachmentCount = 1;

		for(uint32_t i=0; i < vkcontext->scImgCount; i++)
		{
			fbInfo.pAttachments = &vkcontext->scImageViews[i];
			VK_CHECK(vkCreateFramebuffer(vkcontext->device, &fbInfo, nullptr, &vkcontext->framebuffers[i]));
		}
		
	}

	//Pipeline Layout
	{
		VkPipelineLayoutCreateInfo layoutInfo{};
		layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		VK_CHECK(vkCreatePipelineLayout(vkcontext->device, &layoutInfo, nullptr, &vkcontext->pipeLayout));
	}

	// Pipeline
	{
		VkShaderModule vertexShader, fragmentShader;

		uint32_t sizeInBytes;
		char* code{nullptr};
		VkShaderModuleCreateInfo shaderInfo{};
		shaderInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;

		code = platform_read_file(L"assets/shaders/shader.vert.spv", &sizeInBytes);
		shaderInfo.pCode = reinterpret_cast<uint32_t*>(code);
		shaderInfo.codeSize = sizeInBytes;
		VK_CHECK(vkCreateShaderModule(vkcontext->device, &shaderInfo, nullptr, &vertexShader));
		// TODO: Suballocation from main allocation
		delete code;

		code = platform_read_file(L"assets/shaders/shader.frag.spv", &sizeInBytes);
		shaderInfo.pCode = reinterpret_cast<uint32_t*>(code);
		shaderInfo.codeSize = sizeInBytes;
		VK_CHECK(vkCreateShaderModule(vkcontext->device, &shaderInfo, nullptr, &fragmentShader));
		// TODO: Suballocation from main allocation
		delete code;

		VkPipelineShaderStageCreateInfo vertexStage{};
		vertexStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		vertexStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
		vertexStage.pName = "main";
		vertexStage.module = vertexShader;

		VkPipelineShaderStageCreateInfo fragStage{};
		fragStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		fragStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		fragStage.pName = "main";
		fragStage.module = fragmentShader;

		VkPipelineShaderStageCreateInfo shaderStages[] = {
			vertexStage,
			fragStage
		};

		VkPipelineVertexInputStateCreateInfo vertexInputState{};
		vertexInputState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

		VkPipelineColorBlendAttachmentState colorAttachment{};
		colorAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		colorAttachment.blendEnable = VK_FALSE;


		VkPipelineColorBlendStateCreateInfo colorBlendState{};
		colorBlendState.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		colorBlendState.pAttachments = &colorAttachment;
		colorBlendState.attachmentCount = 1;

		VkPipelineRasterizationStateCreateInfo rasterizationState{};
		rasterizationState.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterizationState.frontFace = VK_FRONT_FACE_CLOCKWISE;
		rasterizationState.cullMode = VK_CULL_MODE_BACK_BIT;
		rasterizationState.polygonMode = VK_POLYGON_MODE_FILL;
		rasterizationState.lineWidth = 1.0f;

		VkPipelineMultisampleStateCreateInfo multisampleState{};
		multisampleState.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisampleState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

		VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
		inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

		VkRect2D scissors{};
		VkViewport viewport{};

		VkPipelineViewportStateCreateInfo viewportState{};
		viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewportState.scissorCount = 1;
		viewportState.pScissors = &scissors;
		viewportState.viewportCount = 1;
		viewportState.pViewports = &viewport;

		VkDynamicState dynamicStates[] = {
			VK_DYNAMIC_STATE_VIEWPORT,
			VK_DYNAMIC_STATE_SCISSOR
		};
		VkPipelineDynamicStateCreateInfo dynamicState{};
		dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynamicState.pDynamicStates = dynamicStates;
		dynamicState.dynamicStateCount = std::size(dynamicStates);

		VkGraphicsPipelineCreateInfo pipeInfo{};
		pipeInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipeInfo.layout = vkcontext->pipeLayout;
		pipeInfo.renderPass = vkcontext->renderpass;
		pipeInfo.pVertexInputState = &vertexInputState;
		pipeInfo.pColorBlendState = &colorBlendState;
		pipeInfo.pStages = shaderStages;
		pipeInfo.stageCount = std::size(shaderStages);
		pipeInfo.pRasterizationState = &rasterizationState;
		pipeInfo.pViewportState = &viewportState;
		pipeInfo.pDynamicState= &dynamicState;
		pipeInfo.pMultisampleState = &multisampleState;
		pipeInfo.pInputAssemblyState = &inputAssembly;

		VK_CHECK(vkCreateGraphicsPipelines(vkcontext->device, nullptr, 1, &pipeInfo, nullptr, &vkcontext->pipeline));
	}

	// Command Pool
	{
		VkCommandPoolCreateInfo poolInfo{};
		poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		poolInfo.queueFamilyIndex = vkcontext->graphicsIndex;
		VK_CHECK(vkCreateCommandPool(vkcontext->device, &poolInfo, nullptr, &vkcontext->commandPool));
	}

	// Sync Objects
	{
		VkSemaphoreCreateInfo semaInfo{};
		semaInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		VK_CHECK(vkCreateSemaphore(vkcontext->device, &semaInfo, nullptr, &vkcontext->acquireSemaphore));
		VK_CHECK(vkCreateSemaphore(vkcontext->device, &semaInfo, nullptr, &vkcontext->submitSemaphore));
	}

	// Create Image
	{
		uint32_t fileSize;
		DDSFile* texture_data = reinterpret_cast<DDSFile*>(platform_read_file(L"assets/textures/kyaru.dds", &fileSize));

		// todo assertion

		VkImageCreateInfo imgInfo{};
		imgInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imgInfo.mipLevels = 1;
		imgInfo.arrayLayers = 1;
		imgInfo.imageType = VK_IMAGE_TYPE_2D;
		imgInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
		imgInfo.extent = {texture_data->header.width, texture_data->header.width, 1};
		imgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imgInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		// imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

		VK_CHECK(vkCreateImage(vkcontext->device, &imgInfo, nullptr, &vkcontext->image));

	}

	return true;
}

bool vk_render(VkContext* vkcontext)
{
	uint32_t imgIndex;
	VK_CHECK(vkAcquireNextImageKHR(vkcontext->device, vkcontext->swapchain, 0, vkcontext->acquireSemaphore, nullptr, &imgIndex));

	VkCommandBuffer cmd;
	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandBufferCount = 1;
	allocInfo.commandPool = vkcontext->commandPool;
	VK_CHECK(vkAllocateCommandBuffers(vkcontext->device, &allocInfo, &cmd));
	
	VkCommandBufferBeginInfo beginInfo = cmd_begin_info();
	VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo));

	VkClearValue clearValue{};
	clearValue.color = {1, 1, 0, 1};

	VkRenderPassBeginInfo rpBeginInfo{};
	rpBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	rpBeginInfo.renderPass = vkcontext->renderpass;
	rpBeginInfo.renderArea.extent = vkcontext->screenSize;
	rpBeginInfo.framebuffer = vkcontext->framebuffers[imgIndex];
	rpBeginInfo.pClearValues = &clearValue;
	rpBeginInfo.clearValueCount = 1;

	vkCmdBeginRenderPass(cmd, &rpBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

	// Rendering Commands
	{
		VkRect2D scissor{};
		scissor.extent = vkcontext->screenSize;

		VkViewport viewport{};
		viewport.height = vkcontext->screenSize.height;
		viewport.width = vkcontext->screenSize.width;
		viewport.maxDepth = 1.0f;

		vkCmdSetScissor(cmd, 0, 1, &scissor);
		vkCmdSetViewport(cmd, 0, 1, &viewport);

		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, vkcontext->pipeline);
		vkCmdDraw(cmd, 3, 1, 0, 0);

	}
	vkCmdEndRenderPass(cmd);

	VK_CHECK(vkEndCommandBuffer(cmd));

	VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.pWaitDstStageMask = &waitStage;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &cmd;
	submitInfo.pSignalSemaphores = &vkcontext->submitSemaphore;
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = &vkcontext->acquireSemaphore;
	submitInfo.waitSemaphoreCount = 1;
	VK_CHECK(vkQueueSubmit(vkcontext->graphicsQueue, 1, &submitInfo, nullptr));

	VkPresentInfoKHR presentInfo{};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.pSwapchains = &vkcontext->swapchain;
	presentInfo.swapchainCount = 1;
	presentInfo.pImageIndices = &imgIndex;
	presentInfo.pWaitSemaphores = &vkcontext->submitSemaphore;
	presentInfo.waitSemaphoreCount = 1;
	VK_CHECK(vkQueuePresentKHR(vkcontext->graphicsQueue, &presentInfo))
	
	VK_CHECK(vkDeviceWaitIdle(vkcontext->device));

	vkFreeCommandBuffers(vkcontext->device, vkcontext->commandPool, allocInfo.commandBufferCount, &cmd);

	return true;
}
