#pragma once

#include <vulkan/vulkan.h>
#include <tuple>
#include <vector>

namespace mu
{
	namespace vk
	{
		template<typename T, typename DELETER, typename... ARGS>
		class VkHandle
		{
			T m_handle;
			std::tuple<ARGS...> m_args;
			bool m_do_delete;

			template<std::size_t ...I>
			void CallHelper(std::index_sequence<I...>)
			{
				DELETER d;
				d(m_handle, std::get<I>(m_args)...);
			}

			void Delete()
			{
				if (m_handle)
				{
					CallHelper(std::index_sequence_for<ARGS...>());
				}
			}

		public:
			VkHandle(T h, std::tuple<ARGS...> args)
				: m_handle(h)
				, m_args(args...)
				, m_do_delete(h != nullptr)
			{
			}

			VkHandle(T h, ARGS... args)
				: m_handle(h)
				, m_args(args...)
				, m_do_delete(h != nullptr)
			{
			}

			VkHandle(ARGS... args)
				: VkHandle(nullptr, args...)
			{
			}

			VkHandle()
				: m_handle(nullptr)
				, m_args(std::tuple<ARGS...>())
			{
			}
				
			VkHandle(VkHandle&& other)
				: m_handle(other.m_handle)
				, m_args(other.m_args)
				, m_do_delete(other.m_do_delete)
			{
				other.Release();
			}

			~VkHandle()
			{
				if (m_do_delete)
				{
					Delete();
				}
			}

			VkHandle& operator=(VkHandle&& other)
			{
				Reset();
				m_handle = std::move(other.m_handle);
				m_args = std::move(other.m_args);
				m_do_delete = other.m_do_delete;
				other.m_do_delete = false;
				return *this;
			}
			VkHandle& operator=(const VkHandle&) = delete;

			operator T() const { return m_handle; }

			T* Replace()
			{
				Reset();
				m_do_delete = true;
				return &m_handle;
			}

			void Reset()
			{
				if (m_do_delete)
				{
					Delete();
				}
				m_do_delete = false;
				m_handle = nullptr;
			}

			T Release()
			{
				m_do_delete = false;
				T old = m_handle;
				m_handle = nullptr;
				return old;
			}
		};

		namespace deleters
		{
			struct Instance
			{
				void operator()(VkInstance instance, VkAllocationCallbacks* alloc_callbacks)
				{
					vkDestroyInstance(instance, alloc_callbacks);
				}
			};

			struct SurfaceKHR
			{
				void operator()(VkSurfaceKHR surface, VkInstance instance, VkAllocationCallbacks* alloc_callbacks)
				{
					vkDestroySurfaceKHR(instance, surface, alloc_callbacks);
				}
			};

			struct DebugReportCallbackEXT
			{
				void operator()(VkDebugReportCallbackEXT callback, PFN_vkDestroyDebugReportCallbackEXT func, VkInstance instance, VkAllocationCallbacks* alloc_callbacks)
				{
					func(instance, callback, alloc_callbacks);
				}
			};

			struct Device
			{
				void operator()(VkDevice device, VkAllocationCallbacks* alloc_callbacks)
				{
					vkDestroyDevice(device, alloc_callbacks);
				}
			};

			struct SwapchainKHR
			{
				void operator()(VkSwapchainKHR swap_chain, VkDevice device, VkAllocationCallbacks* alloc_callbacks)
				{
					vkDestroySwapchainKHR(device, swap_chain, alloc_callbacks);
				}
			};

			struct ImageView
			{
				void operator()(VkImageView image_view, VkDevice device, VkAllocationCallbacks* alloc_callbacks)
				{
					vkDestroyImageView(device, image_view, alloc_callbacks);
				}
			};
		}

		// special
		using Instance					= VkHandle<VkInstance,					deleters::Instance,					VkAllocationCallbacks*>;
		using Device					= VkHandle<VkDevice,					deleters::Device,					VkAllocationCallbacks*>;
		using DebugReportCallbackEXT = VkHandle<VkDebugReportCallbackEXT, deleters::DebugReportCallbackEXT, PFN_vkDestroyDebugReportCallbackEXT, VkInstance, VkAllocationCallbacks*>;

		// instance-deleted
		using SurfaceKHR				= VkHandle<VkSurfaceKHR,				deleters::SurfaceKHR,				VkInstance, VkAllocationCallbacks*>;
		
		// device-deleted
		using SwapchainKHR				= VkHandle<VkSwapchainKHR,				deleters::SwapchainKHR,				VkDevice, VkAllocationCallbacks*>;
		using ImageView					= VkHandle<VkImageView,					deleters::ImageView,				VkDevice, VkAllocationCallbacks*>;

		std::vector<VkLayerProperties>			EnumerateInstanceLayerProperties();
		std::vector<VkExtensionProperties>		EnumerateInstanceExtensionProperties(const char* layer_name);
		std::vector<VkExtensionProperties>		EnumerateDeviceExtensionProperties(VkPhysicalDevice device);
		std::vector<VkPhysicalDevice>			EnumeratePhysicalDevices(VkInstance instance);
		std::vector<VkQueueFamilyProperties>	GetPhysicalDeviceQueueFamilyProperties(VkPhysicalDevice device);
		std::vector<VkSurfaceFormatKHR>			GetPhysicalDeviceSurfaceFormatsKHR(VkPhysicalDevice device, VkSurfaceKHR surface);
		std::vector<VkPresentModeKHR>			GetPhysicalDeviceSurfacePresentModesKHR(VkPhysicalDevice device, VkSurfaceKHR surface);
		std::vector<VkImage>					GetSwapchainImagesKHR(VkDevice device, VkSwapchainKHR swap_chain);

		struct SwapChainSupport
		{
			VkSurfaceCapabilitiesKHR		capabilities;
			std::vector<VkSurfaceFormatKHR> surface_formats;
			std::vector<VkPresentModeKHR>	present_modes;
		};
		SwapChainSupport QuerySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface);
		
		inline bool ExtentWithin(VkExtent2D extent, VkExtent2D min, VkExtent2D max)
		{
			return extent.width >= min.width && extent.width <= max.width
				&& extent.height >= min.height && extent.height <= max.height;
		}
	}
}