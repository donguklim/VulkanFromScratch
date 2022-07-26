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
#include "dds_structs.h"

#include "vk_types.h"
#include "vk_init.cpp"

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
	VkCommandBuffer cmd;

	// todo: will be inside an array
	Image image;

	Buffer stagingBuffer;

	VkDescriptorPool descPool;

	VkSampler sampler;
	VkDescriptorSet descSet;
	VkDescriptorSetLayout setLayout;
	VkPipelineLayout pipeLayout;
	VkPipeline pipeline;

	VkSemaphore submitSemaphore;
	VkSemaphore acquireSemaphore;
	VkFence imgAvailableFence;

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

	// Create Descriptor Set Layouts
	{
		VkDescriptorSetLayoutBinding binding{};
		binding.binding = 0;
		binding.descriptorCount = 1;
		binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

		VkDescriptorSetLayoutCreateInfo layoutInfo{};
		layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutInfo.bindingCount = 1;
		layoutInfo.pBindings = &binding;
		VK_CHECK(vkCreateDescriptorSetLayout(vkcontext->device, &layoutInfo, nullptr, &vkcontext->setLayout));
	}

	//Pipeline Layout
	{
		VkPipelineLayoutCreateInfo layoutInfo{};
		layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		layoutInfo.setLayoutCount = 1;
		layoutInfo.pSetLayouts = &vkcontext->setLayout;
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
	// Command Buffer
	{
		VkCommandBufferAllocateInfo allocInfo = cmd_alloc_info(vkcontext->commandPool);
		VK_CHECK(vkAllocateCommandBuffers(vkcontext->device, &allocInfo, &vkcontext->cmd));
	}

	// Sync Objects
	{
		VkSemaphoreCreateInfo semaInfo{};
		semaInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		VK_CHECK(vkCreateSemaphore(vkcontext->device, &semaInfo, nullptr, &vkcontext->acquireSemaphore));
		VK_CHECK(vkCreateSemaphore(vkcontext->device, &semaInfo, nullptr, &vkcontext->submitSemaphore));

		auto fenceInfo = fence_info(VK_FENCE_CREATE_SIGNALED_BIT);
		VK_CHECK(vkCreateFence(vkcontext->device, &fenceInfo, nullptr, &vkcontext->imgAvailableFence));
	}

	// Staging Buffer
	{
		VkBufferCreateInfo bufferInfo{};
		bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		bufferInfo.size = MB(1);
		VK_CHECK(vkCreateBuffer(vkcontext->device, &bufferInfo, nullptr, &vkcontext->stagingBuffer.buffer));

		
		VkMemoryRequirements memRequirements{};
		vkGetBufferMemoryRequirements(vkcontext->device, vkcontext->stagingBuffer.buffer, &memRequirements);

		VkPhysicalDeviceMemoryProperties gpuMemProps{};
		vkGetPhysicalDeviceMemoryProperties(vkcontext->gpu, &gpuMemProps);

		VkMemoryAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = MB(1);
		for(uint32_t i = 0; i< gpuMemProps.memoryTypeCount; i++)
		{
			if (memRequirements.memoryTypeBits & (1 << i) && 
				(gpuMemProps.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) == 
				(VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT))
			{
				allocInfo.memoryTypeIndex = i;
			}
		}

		VK_CHECK(vkAllocateMemory(vkcontext->device, &allocInfo, nullptr, &vkcontext->stagingBuffer.memory));
		VK_CHECK(vkMapMemory(vkcontext->device, vkcontext->stagingBuffer.memory, 0, MB(1), 0, &vkcontext->stagingBuffer.data));
		VK_CHECK(vkBindBufferMemory(vkcontext->device, vkcontext->stagingBuffer.buffer, vkcontext->stagingBuffer.memory, 0));
	}

	// Create Image
	{
		uint32_t fileSize;
		DDSFile* texture_data = reinterpret_cast<DDSFile*>(platform_read_file(L"assets/textures/kyaru.dds", &fileSize));
		uint32_t textureSize = texture_data->header.width * texture_data->header.height * 4;

		memcpy(vkcontext->stagingBuffer.data, &texture_data->dataBegin, textureSize);

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

		VK_CHECK(vkCreateImage(vkcontext->device, &imgInfo, nullptr, &vkcontext->image.image));

		VkMemoryRequirements memRequirements{};
		vkGetImageMemoryRequirements(vkcontext->device, vkcontext->image.image, &memRequirements);

		VkPhysicalDeviceMemoryProperties gpuMemProps{};
		vkGetPhysicalDeviceMemoryProperties(vkcontext->gpu, &gpuMemProps);

		VkMemoryAllocateInfo allocInfo{};
		for(uint32_t i = 0; i< gpuMemProps.memoryTypeCount; i++)
		{
			if (memRequirements.memoryTypeBits & (1 << i) && 
				(gpuMemProps.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) == VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
			{
				allocInfo.memoryTypeIndex = i;
			}
		}

		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = textureSize;
		VK_CHECK(vkAllocateMemory(vkcontext->device, &allocInfo, nullptr, &vkcontext->image.memory));
		VK_CHECK(vkBindImageMemory(vkcontext->device, vkcontext->image.image, vkcontext->image.memory, 0));

		VkCommandBuffer cmd;
		VkCommandBufferAllocateInfo cmdAlloc = cmd_alloc_info(vkcontext->commandPool);
		VK_CHECK(vkAllocateCommandBuffers(vkcontext->device, &cmdAlloc, &cmd));
		

		VkCommandBufferBeginInfo beginInfo = cmd_begin_info();
		VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo));

		VkImageSubresourceRange range{};
		range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		range.layerCount = 1;
		range.levelCount = 1;

		// Transition Layout to Transfer optimal
		VkImageMemoryBarrier imgMemBarrier{};
		imgMemBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		imgMemBarrier.image = vkcontext->image.image;
		imgMemBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imgMemBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		imgMemBarrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		imgMemBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		imgMemBarrier.subresourceRange = range;

		vkCmdPipelineBarrier(
			cmd, 
			VK_PIPELINE_STAGE_TRANSFER_BIT, 
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			0, 0, nullptr, 0, nullptr, 1, &imgMemBarrier
		);
		

		VkBufferImageCopy copyRegion{};
		copyRegion.imageExtent = {texture_data->header.width, texture_data->header.height, 1};
		copyRegion.imageSubresource.layerCount = 1;
		copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		vkCmdCopyBufferToImage(cmd, vkcontext->stagingBuffer.buffer, vkcontext->image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

		imgMemBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		imgMemBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imgMemBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		imgMemBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		vkCmdPipelineBarrier(
			cmd, 
			VK_PIPELINE_STAGE_TRANSFER_BIT, 
			VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
			0, 0, nullptr, 0, nullptr, 1, &imgMemBarrier
		);
		
		VK_CHECK(vkEndCommandBuffer(cmd));

		VkFence uploadFence;
		VkFenceCreateInfo fenceInfo = fence_info();
		VK_CHECK(vkCreateFence(vkcontext->device, &fenceInfo, nullptr, &uploadFence));

		VkSubmitInfo submitInfo = submit_info(&cmd);
		VK_CHECK(vkQueueSubmit(vkcontext->graphicsQueue, 1, &submitInfo, uploadFence));

		VK_CHECK(vkWaitForFences(vkcontext->device, 1, &uploadFence, true, UINT64_MAX));
	}

	// Create Image View
	{
		VkImageViewCreateInfo viewInfo{};
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image = vkcontext->image.image;
		viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
		viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		viewInfo.subresourceRange.layerCount = 1;
		viewInfo.subresourceRange.levelCount = 1;
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		VK_CHECK(vkCreateImageView(vkcontext->device, &viewInfo, nullptr, &vkcontext->image.view));
	}

	// Create Sampler
	{
		VkSamplerCreateInfo samplerInfo{};
		samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerInfo.minFilter = VK_FILTER_NEAREST;
		samplerInfo.magFilter = VK_FILTER_NEAREST;
		// samplerInfo.minFilter = VK_FILTER_LINEAR;
		// samplerInfo.magFilter = VK_FILTER_LINEAR;

		VK_CHECK(vkCreateSampler(vkcontext->device, &samplerInfo, nullptr, &vkcontext->sampler));
	}

	// Create Descriptor Pool
	{
		VkDescriptorPoolSize poolSize{};
		poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		poolSize.descriptorCount = 1;

		VkDescriptorPoolCreateInfo poolInfo{};
		poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.maxSets = 1;
		poolInfo.poolSizeCount = 1;
		poolInfo.pPoolSizes = &poolSize;
		VK_CHECK(vkCreateDescriptorPool(vkcontext->device, &poolInfo, nullptr, &vkcontext->descPool));
	}

	// Create Descriptor Set
	{
		VkDescriptorSetAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.pSetLayouts = &vkcontext->setLayout;
		allocInfo.descriptorSetCount = 1;
		allocInfo.descriptorPool = vkcontext->descPool; 
		VK_CHECK(vkAllocateDescriptorSets(vkcontext->device, &allocInfo, &vkcontext->descSet));
	}

	// Update Descriptor Set
	{
		VkDescriptorImageInfo imgInfo{};
		imgInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imgInfo.imageView = vkcontext->image.view;
		imgInfo.sampler = vkcontext->sampler;

		VkWriteDescriptorSet write{};
		write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		write.dstSet = vkcontext->descSet;
		write.pImageInfo = &imgInfo;
		write.dstBinding = 0;
		write.descriptorCount = 1;
		write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		vkUpdateDescriptorSets(vkcontext->device, 1, &write, 0, nullptr);
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

		vkCmdSetViewport(cmd, 0, 1, &viewport);
		vkCmdSetScissor(cmd, 0, 1, &scissor);

		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, vkcontext->pipeLayout, 0, 1, &vkcontext->descSet, 0, nullptr);
		

		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, vkcontext->pipeline);
		vkCmdDraw(cmd, 6, 1, 0, 0);

	}
	vkCmdEndRenderPass(cmd);

	VK_CHECK(vkEndCommandBuffer(cmd));

	VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

	VkSubmitInfo submitInfo = submit_info(&cmd);
	submitInfo.pWaitDstStageMask = &waitStage;
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
