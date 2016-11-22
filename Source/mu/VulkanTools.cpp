#include "VulkanTools.h"

namespace mu 
{
	namespace vk 
	{
		std::vector<VkLayerProperties> EnumerateInstanceLayerProperties()
		{
			uint32_t num_available_layers = 0;
			vkEnumerateInstanceLayerProperties(&num_available_layers, nullptr);
			std::vector<VkLayerProperties> available_layers{ num_available_layers };
			vkEnumerateInstanceLayerProperties(&num_available_layers, available_layers.data());
			return std::move(available_layers);
		}

		std::vector<VkExtensionProperties> EnumerateInstanceExtensionProperties(const char* layer_name)
		{
			uint32_t num_available_extensions = 0;
			vkEnumerateInstanceExtensionProperties(nullptr, &num_available_extensions, nullptr);
			std::vector<VkExtensionProperties> extensions_properties{ num_available_extensions };
			vkEnumerateInstanceExtensionProperties(nullptr, &num_available_extensions, extensions_properties.data());
			return std::move(extensions_properties);
		}

		std::vector<VkExtensionProperties> EnumerateDeviceExtensionProperties(VkPhysicalDevice device)
		{
			uint32_t num_extensions = 0;
			vkEnumerateDeviceExtensionProperties(device, nullptr, &num_extensions, nullptr);
			std::vector<VkExtensionProperties> extensions_properties{ num_extensions };
			vkEnumerateDeviceExtensionProperties(device, nullptr, &num_extensions, extensions_properties.data());
			return std::move(extensions_properties);
		}

		std::vector<VkPhysicalDevice> EnumeratePhysicalDevices(VkInstance instance)
		{
			uint32_t device_count = 0;
			vkEnumeratePhysicalDevices(instance, &device_count, nullptr);
			std::vector<VkPhysicalDevice> devices{ device_count };
			vkEnumeratePhysicalDevices(instance, &device_count, devices.data());
			return std::move(devices);
		}
		std::vector<VkQueueFamilyProperties> GetPhysicalDeviceQueueFamilyProperties(VkPhysicalDevice device)
		{
			uint32_t num_queue_families = 0;
			vkGetPhysicalDeviceQueueFamilyProperties(device, &num_queue_families, nullptr);
			std::vector<VkQueueFamilyProperties> queue_families{ num_queue_families };
			vkGetPhysicalDeviceQueueFamilyProperties(device, &num_queue_families, queue_families.data());
			return std::move(queue_families);
		}
		std::vector<VkSurfaceFormatKHR> GetPhysicalDeviceSurfaceFormatsKHR(VkPhysicalDevice device, VkSurfaceKHR surface)
		{
			uint32_t count = 0;
			vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &count, nullptr);
			std::vector<VkSurfaceFormatKHR> formats{ count };
			vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &count, formats.data());
			return std::move(formats);
		}
		std::vector<VkPresentModeKHR> GetPhysicalDeviceSurfacePresentModesKHR(VkPhysicalDevice device, VkSurfaceKHR surface)
		{
			uint32_t count = 0;
			vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &count, nullptr);
			std::vector<VkPresentModeKHR> modes{ count };
			vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &count, modes.data());
			return std::move(modes);
		}
		std::vector<VkImage> GetSwapchainImagesKHR(VkDevice device, VkSwapchainKHR swapchain)
		{
			uint32_t image_count = 0;
			vkGetSwapchainImagesKHR(device, swapchain, &image_count, nullptr);
			std::vector<VkImage> images{ image_count };
			vkGetSwapchainImagesKHR(device, swapchain, &image_count, images.data());
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