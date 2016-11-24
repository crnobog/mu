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
	mu::dbg::Log(pMessage);
	return VK_FALSE;
}

void CreateVulkanInstance(mu::vk::Instance& out_instance)
{
	Array<const char*> instance_extensions;
	{
		uint32_t count = 0;
		const char** extensions = glfwGetRequiredInstanceExtensions(&count);

		for (size_t i : mu::Indices(count))
		{
			instance_extensions.Emplace(extensions[i]);
		}

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

void RegisterDebugCallback(VkInstance instance, mu::vk::DebugReportCallbackEXT& out_debug_callback)
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
		VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT | VK_DEBUG_REPORT_INFORMATION_BIT_EXT,
		VkDebugCallback,
		nullptr
	};
	out_debug_callback = mu::vk::DebugReportCallbackEXT{ destroy_func, instance, nullptr };
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
	Array<VkPhysicalDevice> devices = mu::vk::EnumeratePhysicalDevices(instance);

	for (VkPhysicalDevice device : devices)
	{		
		{
			Array<VkExtensionProperties> available_extensions = mu::vk::EnumerateDeviceExtensionProperties(device);
			bool all_found = true;
			for (const char* needed_ext : required_extensions)
			{
				auto f = mu::Find(mu::Range(available_extensions), [needed_ext](const VkExtensionProperties& ext) { return strcmp(ext.extensionName, needed_ext) == 0; });
				all_found = all_found && !f.IsEmpty();
			}

			if (!all_found) { continue; }
		}

		mu::vk::SwapChainSupport swap_chain = mu::vk::QuerySwapChainSupport(device, surface);
		if (swap_chain.surface_formats.IsEmpty() || swap_chain.present_modes.IsEmpty())
		{
			continue;
		}

		Array<VkQueueFamilyProperties> queue_props = mu::vk::GetPhysicalDeviceQueueFamilyProperties(device);
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

			mu::dbg::Log("Using physical device: ", selection.m_device_properties.deviceName, ", graphics queue family: ", graphics_index, ", present queue family:", present_index);
			
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
	if (mu::vk::ExtentWithin(surface_caps.currentExtent, surface_caps.minImageExtent, surface_caps.maxImageExtent))
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
	mu::vk::DebugReportCallbackEXT& out_debug_callback,
	mu::vk::Device& out_device,
	VkQueue& out_graphics_queue, VkQueue& out_present_queue)
{
	RegisterDebugCallback(instance, out_debug_callback);
	
	mu::vk::SwapChainSupport swap_chain_support = mu::vk::QuerySwapChainSupport(selected_device.m_device, surface);
	ChooseSurfaceFormat(swap_chain_support.surface_formats);

	float priority = 1.0f;
	auto queue_families = Array<uint32_t>::MakeUnique( selected_device.m_graphics_queue_family, selected_device.m_present_queue_family );
	Array<VkDeviceQueueCreateInfo> queue_create_info;
	for (uint32_t index : queue_families)
	{
		queue_create_info.Add({
			VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
			nullptr,
			0,
			index,
			1,
			&priority });
	}

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
	mu::vk::SwapchainKHR handle;
	Array<VkImage> images;
	Array<mu::vk::ImageView> image_views;
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

	mu::vk::SwapChainSupport swap_chain_support = mu::vk::QuerySwapChainSupport(device_selection.m_device, surface);
	VkSurfaceFormatKHR surface_format = ChooseSurfaceFormat(swap_chain_support.surface_formats);
	VkPresentModeKHR present_mode = ChoosePresentMode(swap_chain_support.present_modes);
	VkExtent2D extent = ChooseSwapExtent(swap_chain_support.capabilities, fb_width, fb_height);

	uint32_t image_count = 1 + swap_chain_support.capabilities.minImageCount;
	if (swap_chain_support.capabilities.maxImageCount > 0)
	{
		image_count = Min(image_count, swap_chain_support.capabilities.maxImageCount);
	}
	Array<uint32_t> queue_family_indices{ device_selection.m_graphics_queue_family, device_selection.m_present_queue_family };
	// TODO:
	//queue_family_indices.erase(std::unique(queue_family_indices.begin(), queue_family_indices.end()), queue_family_indices.end());

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
	mu::vk::SwapchainKHR out_swapchain{ device, nullptr };
	if (vkCreateSwapchainKHR(device, &swapchain_create_info, nullptr, out_swapchain.Replace()) != VK_SUCCESS)
	{
		throw std::runtime_error("Failed to create swap chain");
	}
	Array<VkImage> images = mu::vk::GetSwapchainImagesKHR(device, out_swapchain);
	Array<mu::vk::ImageView> image_views;
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
		mu::vk::ImageView view{ device, nullptr };
		vkCreateImageView(device, &image_view_create_info, nullptr, view.Replace());
		image_views.Add(std::move(view));
	}

	return{ std::move(out_swapchain), std::move(images), std::move(image_views), surface_format.format, extent };
}

struct A
{
	A(std::initializer_list<int> a)
	{
		mu::dbg::Log("initializer list constructor");
	}
	A(int a)
	{
		mu::dbg::Log("int constructor");
	}
};

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

	SCOPE_EXIT(glfwDestroyWindow(window));

	mu::vk::Instance instance;
	mu::vk::DebugReportCallbackEXT debug_callbacks;
	mu::vk::Device device;
	mu::vk::SurfaceKHR surface;
	Swapchain swapchain;
	VkQueue graphics_queue, present_queue;
	try
	{
		CreateVulkanInstance(instance);
		surface = mu::vk::SurfaceKHR{ instance, nullptr };
		VkResult err = glfwCreateWindowSurface(instance, window, nullptr, surface.Replace());
		if (err)
		{
			throw std::runtime_error("Failed to create surface");
		}

		Array<const char*> device_extensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
		PhysicalDeviceSelection selected_device = SelectPhysicalDevice(device_extensions, instance, surface);
		CreateDevice(selected_device, device_extensions, window, instance, surface, debug_callbacks, device, graphics_queue, present_queue);
		swapchain = CreateSwapChain(window, selected_device, device, surface);		
	}
	catch (const std::runtime_error& e)
	{
		mu::dbg::Log("InitVulkan error: ", e.what());
		return 1;
	}

	while (!glfwWindowShouldClose(window))
	{
		glfwPollEvents();
	}
	
	return 0;
}