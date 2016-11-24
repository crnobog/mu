#include "VulkanTools.h"

namespace mu 
{
	namespace vk 
	{
		Array<VkLayerProperties> EnumerateInstanceLayerProperties()
		{
			uint32_t num_available_layers = 0;
			vkEnumerateInstanceLayerProperties(&num_available_layers, nullptr);
			auto available_layers = Array<VkLayerProperties>::MakeUninitialized(num_available_layers);
			vkEnumerateInstanceLayerProperties(&num_available_layers, available_layers.Data());
			return std::move(available_layers);
		}

		Array<VkExtensionProperties> EnumerateInstanceExtensionProperties(const char* layer_name)
		{
			uint32_t num_available_extensions = 0;
			vkEnumerateInstanceExtensionProperties(nullptr, &num_available_extensions, nullptr);
			auto extensions_properties = Array<VkExtensionProperties>::MakeUninitialized( num_available_extensions );
			vkEnumerateInstanceExtensionProperties(nullptr, &num_available_extensions, extensions_properties.Data());
			return std::move(extensions_properties);
		}

		Array<VkExtensionProperties> EnumerateDeviceExtensionProperties(VkPhysicalDevice device)
		{
			uint32_t num_extensions = 0;
			vkEnumerateDeviceExtensionProperties(device, nullptr, &num_extensions, nullptr);
			auto extensions_properties = Array<VkExtensionProperties>::MakeUninitialized( num_extensions );
			vkEnumerateDeviceExtensionProperties(device, nullptr, &num_extensions, extensions_properties.Data());
			return std::move(extensions_properties);
		}

		Array<VkPhysicalDevice> EnumeratePhysicalDevices(VkInstance instance)
		{
			uint32_t device_count = 0;
			vkEnumeratePhysicalDevices(instance, &device_count, nullptr);
			auto devices = Array<VkPhysicalDevice>::MakeUninitialized( device_count );
			vkEnumeratePhysicalDevices(instance, &device_count, devices.Data());
			return std::move(devices);
		}
		Array<VkQueueFamilyProperties> GetPhysicalDeviceQueueFamilyProperties(VkPhysicalDevice device)
		{
			uint32_t num_queue_families = 0;
			vkGetPhysicalDeviceQueueFamilyProperties(device, &num_queue_families, nullptr);
			auto queue_families = Array<VkQueueFamilyProperties>::MakeUninitialized( num_queue_families );
			vkGetPhysicalDeviceQueueFamilyProperties(device, &num_queue_families, queue_families.Data());
			return std::move(queue_families);
		}
		Array<VkSurfaceFormatKHR> GetPhysicalDeviceSurfaceFormatsKHR(VkPhysicalDevice device, VkSurfaceKHR surface)
		{
			uint32_t count = 0;
			vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &count, nullptr);
			auto formats = Array<VkSurfaceFormatKHR>::MakeUninitialized( count );
			vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &count, formats.Data());
			return std::move(formats);
		}
		Array<VkPresentModeKHR> GetPhysicalDeviceSurfacePresentModesKHR(VkPhysicalDevice device, VkSurfaceKHR surface)
		{
			uint32_t count = 0;
			vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &count, nullptr);
			auto modes = Array<VkPresentModeKHR>::MakeUninitialized( count );
			vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &count, modes.Data());
			return std::move(modes);
		}
		Array<VkImage> GetSwapchainImagesKHR(VkDevice device, VkSwapchainKHR swapchain)
		{
			uint32_t image_count = 0;
			vkGetSwapchainImagesKHR(device, swapchain, &image_count, nullptr);
			auto images = Array<VkImage>::MakeUninitialized( image_count );
			vkGetSwapchainImagesKHR(device, swapchain, &image_count, images.Data());
			return std::move(images);
		}
		SwapChainSupport QuerySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface)
		{
			SwapChainSupport details = {};
			vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);
			details.surface_formats = GetPhysicalDeviceSurfaceFormatsKHR(device, surface);
			details.present_modes = GetPhysicalDeviceSurfacePresentModesKHR(device, surface);
			return std::move(details);
		}
	}
}