#include <vulkan/vulkan.h>

#include <glfw/glfw3.h>
#include <cstdint>
#include <memory>

#include "Array.h"
#include "Ranges.h"
#include "Algorithms.h"
#include "Debug.h"
#include "Scope.h"
#include "VulkanTools.h"
#include "Utils.h"
#include "Math.h"
#include "FileReader.h"

using std::tuple;
using namespace mu;

VKAPI_ATTR VkBool32 VKAPI_CALL VkDebugCallback(
	VkDebugReportFlagsEXT                       flags,
	VkDebugReportObjectTypeEXT                  objectType,
	uint64_t                                    object,
	size_t                                      location,
	int32_t                                     messageCode,
	const char*                                 pLayerPrefix,
	const char*                                 pMessage,
	void*                                       pUserData)
{
	dbg::Log(pMessage);
	return VK_FALSE;
}

void CreateVulkanInstance(vk::Instance& out_instance)
{
	Array<const char*> instance_extensions;
	{
		uint32_t count = 0;
		const char** extensions = glfwGetRequiredInstanceExtensions(&count);
		instance_extensions.AppendRaw(extensions, count);
		instance_extensions.Emplace(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
	}

	auto app_info = VkApplicationInfo{
		VK_STRUCTURE_TYPE_APPLICATION_INFO, nullptr,
		"mu",	0,
		"mu",	0,
		VK_API_VERSION_1_0
	};

	Array<const char*> layers = { "VK_LAYER_LUNARG_standard_validation" };


	auto instance_create_info = VkInstanceCreateInfo{
		VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, nullptr,
		0,
		&app_info,
		(uint32_t)layers.Num(), layers.Data(),
		(uint32_t)instance_extensions.Num(), instance_extensions.Data()
	};

	if (vkCreateInstance(&instance_create_info, nullptr, out_instance.Replace()) != VK_SUCCESS)
	{
		throw std::runtime_error("vkCreateInstance failed");
	}
}

void RegisterDebugCallback(VkInstance instance, vk::DebugReportCallbackEXT& out_debug_callback)
{
	auto create_func = (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugReportCallbackEXT");
	auto destroy_func = (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugReportCallbackEXT");
	if (!create_func || !destroy_func)
	{
		throw std::runtime_error("Unable to get debug callback registration function pointers");
	}

	VkDebugReportCallbackCreateInfoEXT debug_callback_create_info{
		VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT,
		nullptr,
		VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT /*| VK_DEBUG_REPORT_INFORMATION_BIT_EXT*/,
		VkDebugCallback,
		nullptr
	};
	out_debug_callback = vk::DebugReportCallbackEXT{ destroy_func, instance, nullptr };
	if (create_func(instance, &debug_callback_create_info, nullptr, out_debug_callback.Replace()) != VK_SUCCESS)
	{
		throw std::runtime_error("Unable to register debug callback");
	}
}

struct PhysicalDeviceSelection
{
	VkPhysicalDevice m_device;
	VkPhysicalDeviceProperties m_device_properties;
	uint32_t m_graphics_queue_family;
	uint32_t m_present_queue_family;
};

PhysicalDeviceSelection SelectPhysicalDevice(
	const Array<const char*>& required_extensions,
	VkInstance instance, 
	VkSurfaceKHR surface)
{
	Array<VkPhysicalDevice> devices = vk::EnumeratePhysicalDevices(instance);

	for (VkPhysicalDevice device : devices)
	{		
		{
			Array<VkExtensionProperties> available_extensions = vk::EnumerateDeviceExtensionProperties(device);
			bool all_found = true;
			for (const char* needed_ext : required_extensions)
			{
				auto f = Find(available_extensions, [needed_ext](const VkExtensionProperties& ext) { return strcmp(ext.extensionName, needed_ext) == 0; });
				all_found = all_found && !f.IsEmpty();
			}

			if (!all_found) { continue; }
		}

		vk::SwapChainSupport swap_chain = vk::QuerySwapChainSupport(device, surface);
		if (swap_chain.surface_formats.IsEmpty() || swap_chain.present_modes.IsEmpty())
		{
			continue;
		}

		Array<VkQueueFamilyProperties> queue_props = vk::GetPhysicalDeviceQueueFamilyProperties(device);
		int32_t graphics_index = -1, present_index = -1;
		for (int32_t i = 0; i < queue_props.Num(); ++i)
		{
			if (queue_props[i].queueCount > 0)
			{
				if (graphics_index < 0 && (queue_props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0)
				{
					graphics_index = i;
				}

				VkBool32 supports_present = false;
				vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &supports_present);
				if (present_index < 0 && supports_present)
				{
					present_index = i;
				}
			}
		}

		if (graphics_index >= 0 && present_index >= 0)
		{
			PhysicalDeviceSelection selection{ device,{}, (uint32_t)graphics_index, (uint32_t)present_index};
			vkGetPhysicalDeviceProperties(device, &selection.m_device_properties); 

			dbg::Log("Using physical device: ", selection.m_device_properties.deviceName, ", graphics queue family: ", graphics_index, ", present queue family:", present_index);
			
			return selection;
		}
	}

	throw std::runtime_error("No device available");
}

VkSurfaceFormatKHR ChooseSurfaceFormat(const Array<VkSurfaceFormatKHR>& surface_formats)
{
	if (surface_formats.Num() == 0) throw std::runtime_error("No device formats available");

	if (surface_formats.Num() == 1 && surface_formats[0].format == VK_FORMAT_UNDEFINED)
	{
		return{ VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
	}

	for (const auto& format : surface_formats)
	{
		if (format.format == VK_FORMAT_B8G8R8A8_UNORM && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
		{
			return format;
		}
	}
	return surface_formats[0];
}

VkPresentModeKHR ChoosePresentMode(const Array<VkPresentModeKHR>& present_modes)
{
	for (const auto& mode : present_modes)
	{
		if (mode == VK_PRESENT_MODE_MAILBOX_KHR)
		{
			return mode;
		}
	}
	return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& surface_caps, uint32_t fb_width, uint32_t fb_height)
{
	if (vk::ExtentWithin(surface_caps.currentExtent, surface_caps.minImageExtent, surface_caps.maxImageExtent))
	{
		return surface_caps.currentExtent;
	}
	return{ Clamp(fb_width, surface_caps.minImageExtent.width, surface_caps.maxImageExtent.width), 
		Clamp(fb_height, surface_caps.minImageExtent.height, surface_caps.maxImageExtent.height) };
}

void CreateDevice(
	PhysicalDeviceSelection selected_device,
	const Array<const char*>& device_extensions,
	GLFWwindow* window,
	VkInstance instance,
	VkSurfaceKHR surface,
	vk::Device& out_device,
	VkQueue& out_graphics_queue, VkQueue& out_present_queue)
{	
	vk::SwapChainSupport swap_chain_support = vk::QuerySwapChainSupport(selected_device.m_device, surface);
	ChooseSurfaceFormat(swap_chain_support.surface_formats);

	float priority = 1.0f;
	auto queue_families = Array<uint32_t>::MakeUnique( selected_device.m_graphics_queue_family, selected_device.m_present_queue_family );
	Array<VkDeviceQueueCreateInfo> queue_create_info{ Transform(Range(queue_families), [&](uint32_t index) {
		return VkDeviceQueueCreateInfo{
			VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
			nullptr,
			0,
			index,
			1,
			&priority };
		}
	)};

	VkPhysicalDeviceFeatures device_features = {};
	VkDeviceCreateInfo device_create_info = {
		VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
		nullptr,
		0,
		(uint32_t)queue_create_info.Num(), queue_create_info.Data(),
		0, nullptr,
		(uint32_t)device_extensions.Num(), device_extensions.Data(),
		&device_features
	};
	if (vkCreateDevice(selected_device.m_device, &device_create_info, nullptr, out_device.Replace()) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create logical device");
	}
	vkGetDeviceQueue(out_device, selected_device.m_graphics_queue_family, 0, &out_graphics_queue);
	vkGetDeviceQueue(out_device, selected_device.m_present_queue_family, 0, &out_present_queue);
}

struct Swapchain
{
	vk::SwapchainKHR handle;
	Array<VkImage> images;
	Array<vk::ImageView> image_views;
	VkFormat image_format;
	VkExtent2D extent;
};

Swapchain CreateSwapChain(
	GLFWwindow* window,
	PhysicalDeviceSelection device_selection,
	VkDevice device,
	VkSurfaceKHR surface)
{
	int fb_width = 0, fb_height = 0;
	glfwGetFramebufferSize(window, &fb_width, &fb_height);

	vk::SwapChainSupport swap_chain_support = vk::QuerySwapChainSupport(device_selection.m_device, surface);
	VkSurfaceFormatKHR surface_format = ChooseSurfaceFormat(swap_chain_support.surface_formats);
	VkPresentModeKHR present_mode = ChoosePresentMode(swap_chain_support.present_modes);
	VkExtent2D extent = ChooseSwapExtent(swap_chain_support.capabilities, fb_width, fb_height);

	uint32_t image_count = 1 + swap_chain_support.capabilities.minImageCount;
	if (swap_chain_support.capabilities.maxImageCount > 0)
	{
		image_count = Min(image_count, swap_chain_support.capabilities.maxImageCount);
	}
	auto queue_family_indices = Array<uint32_t>::MakeUnique( device_selection.m_graphics_queue_family, device_selection.m_present_queue_family );

	VkSharingMode sharing_mode = queue_family_indices.Num() == 1 ? VK_SHARING_MODE_EXCLUSIVE : VK_SHARING_MODE_CONCURRENT;

	VkSwapchainCreateInfoKHR swapchain_create_info = {
		VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
		nullptr,
		0, // flags
		surface, // surface
		image_count, // minImageCount
		surface_format.format, // imageFormat
		surface_format.colorSpace, // imageColorSpace
		extent, // imageExtent
		1, // imageArrayLayers
		VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, // imageUsage
		sharing_mode, // imageSharingMode
		(uint32_t)queue_family_indices.Num(), queue_family_indices.Data(), // queueFamilyIndexCount, pQueueFamilyIndices
		swap_chain_support.capabilities.currentTransform, // preTransform
		VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR, // compositeAlpha
		present_mode, // presentMode
		VK_TRUE, // clipped
		VK_NULL_HANDLE, // oldSwapchain
	};
	vk::SwapchainKHR out_swapchain{ device, nullptr };
	if (vkCreateSwapchainKHR(device, &swapchain_create_info, nullptr, out_swapchain.Replace()) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create swap chain");
	}
	Array<VkImage> images = vk::GetSwapchainImagesKHR(device, out_swapchain);
	Array<vk::ImageView> image_views;
	for (VkImage image : images)
	{
		VkImageViewCreateInfo image_view_create_info{
			VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, nullptr,
			0,
			image,
			VK_IMAGE_VIEW_TYPE_2D,
			surface_format.format,
			{ VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY }, // components
			{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 } // subresourcerange
		};
		vk::ImageView view{ device, nullptr };
		vkCreateImageView(device, &image_view_create_info, nullptr, view.Replace());
		image_views.Add(std::move(view));
	}

	return{ std::move(out_swapchain), std::move(images), std::move(image_views), surface_format.format, extent };
}

vk::ShaderModule CreateShaderModule(VkDevice device, const ranges::PointerRange<uint8_t>& code)
{
	VkShaderModuleCreateInfo create_info = {
		VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		nullptr,
		0,
		code.Size(), reinterpret_cast<const uint32_t*>(&code.Front()),
	};

	vk::ShaderModule shader_module{ device, nullptr };
	if (vkCreateShaderModule(device, &create_info, nullptr, shader_module.Replace()) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create shader module");
	}
	return std::move(shader_module);
}

vk::PipelineLayout CreatePipelineLayout(VkDevice device)
{
	VkPipelineLayoutCreateInfo pipeline_create_info = {
		VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		nullptr,
		0,
		0, nullptr,	// descriptor sets 
		0, nullptr,	// push constants
	};

	auto pipeline_layout = vk::PipelineLayout{ device, nullptr };
	if (vkCreatePipelineLayout(device, &pipeline_create_info, nullptr, pipeline_layout.Replace()) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create pipeline layout");
	}
	return pipeline_layout;
}

vk::RenderPass CreateRenderPass(VkDevice device, VkFormat swapchain_format)
{
	VkAttachmentDescription color_attachment = {
		0, // flags
		swapchain_format, 
		VK_SAMPLE_COUNT_1_BIT,
		VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE,	// attachment ops
		VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_DONT_CARE, // stencil ops
		VK_IMAGE_LAYOUT_UNDEFINED, // initial layout
		VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, // final layout
	};

	VkAttachmentReference color_attachment_ref = {
		0, // attachment index
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, // layout
	};

	VkSubpassDescription subpass = {
		0, // flags
		VK_PIPELINE_BIND_POINT_GRAPHICS,
		0, nullptr, // input attachments
		1, &color_attachment_ref, // color atachments
		nullptr, // resolve attachments
		nullptr, // depth stencil attachment
		0, nullptr, // preserve attachments
	};

	//VkSubpassDependency dependency = {
	//	VK_SUBPASS_EXTERNAL, 0, // src/dest subpass
	//	VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, // src/dest stage mask
	//	0, VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, // src/dest access mask
	//	0
	//};

	VkRenderPassCreateInfo render_pass_info = {
		VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
		nullptr,
		0,
		1, &color_attachment,
		1, &subpass,
		//1, &dependency, // dependencies
		0, nullptr
	};

	vk::RenderPass render_pass{ device, nullptr };
	if (vkCreateRenderPass(device, &render_pass_info, nullptr, render_pass.Replace()) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create render pass");
	}
	return std::move(render_pass);
}

vk::Pipeline CreatePipeline(
	VkDevice			device, 
	VkPipelineLayout	pipeline_layout, 
	VkRenderPass		render_pass, 
	VkShaderModule		vert_shader, 
	VkShaderModule		frag_shader, 
	VkExtent2D			viewport_extent)
{
	VkPipelineShaderStageCreateInfo shader_stages[] = {
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			nullptr,
			0,
			VK_SHADER_STAGE_VERTEX_BIT,
			vert_shader,
			"main",
			nullptr
		},
		{
			VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			nullptr,
			0,
			VK_SHADER_STAGE_FRAGMENT_BIT,
			frag_shader,
			"main",
			nullptr
		}
	};
	VkPipelineVertexInputStateCreateInfo vertex_input_info = {
		VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		nullptr,
		0,
		0, nullptr, // vertex binding description
		0, nullptr, // vertex attribute description
	};
	VkPipelineInputAssemblyStateCreateInfo input_assembly_info = {
		VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
		nullptr,
		0,
		VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
		false, // primitive restart enable
	};

	VkViewport viewport = {
		0, 0,
		float(viewport_extent.width), float(viewport_extent.height),
		0.0f, 1.0f, // depth range
	};

	VkRect2D scissor = {
		{ 0, 0 },
		{ viewport_extent.width, viewport_extent.height }
	};

	VkPipelineViewportStateCreateInfo viewport_create_info = {
		VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
		nullptr,
		0,
		1, &viewport,
		1, &scissor
	};
	VkPipelineRasterizationStateCreateInfo raster_state_create_info = {
		VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
		nullptr,
		0,
		false, // depth clamp
		false, // raster discard enable
		VK_POLYGON_MODE_FILL,
		VK_CULL_MODE_NONE,
		VK_FRONT_FACE_CLOCKWISE,
		false, // depth bias enable
		0.0f, // constant depth bias
		0.0f,  // depth bias clamp
		0.0f, // depth bias slope factor
		1.0f, // line width
	};

	VkPipelineMultisampleStateCreateInfo multisample_state_create_info = {
		VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
		nullptr,
		0,
		VK_SAMPLE_COUNT_1_BIT,
		false, // sample shading enable
		1.0f, // min sample shading
		nullptr, // sample mask
		false, // alpha to coverage
		false, // alpha to one
	};

	VkPipelineColorBlendAttachmentState color_blend_attachment_state = {
		false, // blend enable
		VK_BLEND_FACTOR_ONE, // source color blend factor
		VK_BLEND_FACTOR_ZERO, // dest color blend factor
		VK_BLEND_OP_ADD, // color blend op
		VK_BLEND_FACTOR_ONE, // source alpha blend factor
		VK_BLEND_FACTOR_ZERO, // dest alpha blend factor
		VK_BLEND_OP_ADD, // alpha blend op
		VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT, // color mask
	};

	VkPipelineColorBlendStateCreateInfo color_blend_state_create_info = {
		VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		nullptr,
		0,
		false, // logic op enable
		VK_LOGIC_OP_COPY, // logic op
		1, &color_blend_attachment_state, // attachments
		{ 0.0f, 0.0f, 0.0f, 0.0f }, // blend constants
	};

	VkDynamicState dynamic_states[] = {
		//VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_LINE_WIDTH,
	};

	VkPipelineDynamicStateCreateInfo dynamic_state_create_info = {
		VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
		nullptr,
		0,
		sizeof(dynamic_states) / sizeof(VkDynamicState), dynamic_states
	};

	VkGraphicsPipelineCreateInfo pipeline_info = {
		VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		nullptr, 
		0,
		2, shader_stages,
		&vertex_input_info,
		&input_assembly_info,
		nullptr, // tesselation state
		&viewport_create_info,
		&raster_state_create_info,
		&multisample_state_create_info,
		nullptr, // depth stencil info
		&color_blend_state_create_info,
		&dynamic_state_create_info,
		pipeline_layout,
		render_pass,
		0, // subpass
		nullptr, -1 // base pipeline
	};

	vk::Pipeline pipeline{ device, nullptr };
	if (vkCreateGraphicsPipelines(device, nullptr, 1, &pipeline_info, nullptr, pipeline.Replace()) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create pipeline");
	}
	return std::move(pipeline);
}

Array<vk::Framebuffer> CreateFramebuffers(
	VkDevice device,
	VkRenderPass render_pass,
	const Swapchain& swapchain)
{
	auto framebuffers = Array<vk::Framebuffer>::MakeUninitialized(swapchain.image_views.Num());
	FillConstruct(Range(framebuffers), device, nullptr);

	for (std::tuple<const vk::ImageView&, vk::Framebuffer&> pair : Zip(Range(swapchain.image_views), Range(framebuffers)))
	{
		VkImageView attachments[] = { std::get<0>(pair) };

		VkFramebufferCreateInfo framebuffer_info = {
			VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
			nullptr,
			0,
			render_pass,
			1, attachments,
			swapchain.extent.width, swapchain.extent.height,
			1 // layers
		};

		if (vkCreateFramebuffer(device, &framebuffer_info, nullptr, std::get<1>(pair).Replace()) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create framebuffer");
		}
	}

	return std::move(framebuffers);
}

vk::CommandPool CreateCommandPool(VkDevice device, const PhysicalDeviceSelection& device_info)
{
	VkCommandPoolCreateInfo pool_info = {
		VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
		nullptr,
		0,
		device_info.m_graphics_queue_family
	};

	vk::CommandPool command_pool{ device, nullptr };
	if (vkCreateCommandPool(device, &pool_info, nullptr, command_pool.Replace()) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create command pool");
	}
	return std::move(command_pool);
}

Array<VkCommandBuffer> CreateCommandBuffers(VkDevice device, VkCommandPool command_pool, uint32_t count)
{
	auto command_buffers = Array<VkCommandBuffer>::MakeUninitialized(count);
	VkCommandBufferAllocateInfo alloc_info = {
		VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
		nullptr,
		command_pool,
		VK_COMMAND_BUFFER_LEVEL_PRIMARY,
		count,
	};

	if( vkAllocateCommandBuffers(device, &alloc_info, command_buffers.Data()) != VK_SUCCESS )
	{ 
		throw std::runtime_error("Failed to allocate command buffers");
	}
	return std::move(command_buffers);
}

void RecordCommandBuffers(
	ranges::PointerRange<VkCommandBuffer> command_buffers,
	ranges::PointerRange<vk::Framebuffer> framebuffers,
	VkPipeline graphics_pipeline,
	VkRenderPass render_pass,
	VkExtent2D framebuffer_extent)
{
	for (tuple<VkCommandBuffer&, vk::Framebuffer&> pair : Zip(command_buffers, framebuffers))
	{
		VkCommandBuffer command_buffer = std::get<0>(pair);
		VkFramebuffer framebuffer = std::get<1>(pair);

		VkCommandBufferBeginInfo begin_info = {
			VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
			nullptr,
			VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT,
			nullptr // inheritance info
		};
		vkBeginCommandBuffer(command_buffer, &begin_info);
		{
			VkClearValue clear_color = { 0.0f, 0.0f, 0.0f, 1.0f };
			VkRenderPassBeginInfo begin_pass = {
				VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
				nullptr,
				render_pass,
				framebuffer,
				{ {0,0}, framebuffer_extent },
				1, &clear_color
			};
			vkCmdBeginRenderPass(command_buffer, &begin_pass, VK_SUBPASS_CONTENTS_INLINE);
			{
				vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphics_pipeline);
				vkCmdDraw(command_buffer, 3, 1, 0, 0);
			}
			vkCmdEndRenderPass(command_buffer);
		}
		if (vkEndCommandBuffer(command_buffer) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to record command buffer");
		}
	}
}

void CreateSemaphoresRec(VkDevice device, VkSemaphoreCreateInfo& semaphore_info/*, vk::Semaphore& semaphore*/) { }

template<typename... SEMAPHORES>
void CreateSemaphoresRec(VkDevice device, VkSemaphoreCreateInfo& semaphore_info, vk::Semaphore& semaphore, SEMAPHORES&... semaphores)
{
	semaphore = vk::Semaphore{ device, nullptr };
	if (vkCreateSemaphore(device, &semaphore_info, nullptr, semaphore.Replace()) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create semaphore");
	}
	CreateSemaphoresRec(device, semaphore_info, semaphores...);
}

template<typename... SEMAPHORES>
void CreateSemaphores(VkDevice device, SEMAPHORES&... semaphores)
{
	VkSemaphoreCreateInfo semaphore_info = {
		VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
		nullptr,
		0
	};
	
	CreateSemaphoresRec(device, semaphore_info, semaphores...);
}


/*! @brief The function signature for keyboard key callbacks.
*
*  This is the function signature for keyboard key callback functions.
*
*  @param[in] window The window that received the event.
*  @param[in] key The [keyboard key](@ref keys) that was pressed or released.
*  @param[in] scancode The system-specific scancode of the key.
*  @param[in] action `GLFW_PRESS`, `GLFW_RELEASE` or `GLFW_REPEAT`.
*  @param[in] mods Bit field describing which [modifier keys](@ref mods) were
*  held down.
*
*  @sa @ref input_key
*  @sa glfwSetKeyCallback
*
*  @since Added in version 1.0.
*  @glfw3 Added window handle, scancode and modifier mask parameters.
*
*  @ingroup input
*/
bool bAllowAppStart = false;
void GLFW_OnKeyPressed(GLFWwindow*, int, int, int, int)
{
	bAllowAppStart = true;
}

int main(int, char**)
{
	if (!glfwInit())
	{
		return 1;
	}
	SCOPE_EXIT(glfwTerminate());

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
	GLFWwindow* window = glfwCreateWindow(1280, 720, "mu", nullptr, nullptr);
	if (!window)
	{
		return 1;
	}

	glfwSetKeyCallback(window, GLFW_OnKeyPressed);
	while (!glfwWindowShouldClose(window) && !bAllowAppStart)
	{
		glfwPollEvents();
	}

	SCOPE_EXIT(glfwDestroyWindow(window));

	vk::Instance instance;
	vk::DebugReportCallbackEXT debug_callbacks;
	vk::Device device;
	vk::SurfaceKHR surface;
	Swapchain swapchain;
	VkQueue graphics_queue, present_queue;
	vk::ShaderModule vert_shader, frag_shader;
	vk::PipelineLayout pipeline_layout;
	vk::RenderPass render_pass;
	vk::Pipeline pipeline;
	Array<vk::Framebuffer> framebuffers;
	vk::CommandPool command_pool;
	Array<VkCommandBuffer> command_buffers;
	vk::Semaphore image_available_semaphore, render_finished_semaphore;
	try
	{
		CreateVulkanInstance(instance);
		RegisterDebugCallback(instance, debug_callbacks);

		surface = vk::SurfaceKHR{ instance, nullptr };
		VkResult err = glfwCreateWindowSurface(instance, window, nullptr, surface.Replace());
		if (err)
		{
			throw std::runtime_error("Failed to create surface");
		}

		Array<const char*> device_extensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
		PhysicalDeviceSelection selected_device = SelectPhysicalDevice(device_extensions, instance, surface);
		CreateDevice(selected_device, device_extensions, window, instance, surface, device, graphics_queue, present_queue);
		swapchain = CreateSwapChain(window, selected_device, device, surface);

		auto vert_shader_code = LoadFileToArray("../Shaders/Bin/shader.vert.spv");
		vert_shader = CreateShaderModule(device, Range(vert_shader_code));

		auto frag_shader_code = LoadFileToArray("../Shaders/Bin/shader.frag.spv");
		frag_shader = CreateShaderModule(device, Range(frag_shader_code));

		pipeline_layout = CreatePipelineLayout(device);
		render_pass = CreateRenderPass(device, swapchain.image_format);
		pipeline = CreatePipeline(device, pipeline_layout, render_pass, vert_shader, frag_shader, swapchain.extent);
		framebuffers = CreateFramebuffers(device, render_pass, swapchain);
		command_pool = CreateCommandPool(device, selected_device);
		command_buffers = CreateCommandBuffers(device, command_pool, uint32_t(framebuffers.Num()));
		RecordCommandBuffers(Range(command_buffers), Range(framebuffers), pipeline, render_pass, swapchain.extent);
		CreateSemaphores(device, image_available_semaphore, render_finished_semaphore);
	}
	catch (const std::runtime_error& e)
	{
		dbg::Log("InitVulkan error: ", e.what());
		return 1;
	}

	while (!glfwWindowShouldClose(window))
	{
		glfwPollEvents();

		uint32_t image_index = 0;
		vkAcquireNextImageKHR(device, swapchain.handle, UINT64_MAX, image_available_semaphore, nullptr, &image_index);

		VkSemaphore submit_wait_semaphores[] = { image_available_semaphore };
		VkPipelineStageFlags submit_wait_stages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
		VkSemaphore signal_semaphores[] = { render_finished_semaphore };
		VkSubmitInfo submit_info = {
			VK_STRUCTURE_TYPE_SUBMIT_INFO,
			nullptr,
			1, submit_wait_semaphores, submit_wait_stages,
			1, &command_buffers[image_index],
			1, signal_semaphores,
		};

		if (vkQueueSubmit(graphics_queue, 1, &submit_info, nullptr) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to submit command queue");
		}

		VkSemaphore present_wait_list[] = { render_finished_semaphore };
		VkSwapchainKHR present_swapchain[] = { swapchain.handle };
		VkPresentInfoKHR present_info =	{
			VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
			nullptr,
			1, present_wait_list,
			1, present_swapchain, &image_index,
			nullptr
		};
		vkQueuePresentKHR(present_queue, &present_info);
	}
	
	vkDeviceWaitIdle(device);

	return 0;
}