// Copyright (C) 2020 Jérôme Leclercq
// This file is part of the "Nazara Engine - Vulkan Renderer"
// For conditions of distribution and use, see copyright notice in Config.hpp

#include <Nazara/VulkanRenderer/Wrapper/Device.hpp>
#include <Nazara/Core/CallOnExit.hpp>
#include <Nazara/Core/Error.hpp>
#include <Nazara/Core/ErrorFlags.hpp>
#include <Nazara/VulkanRenderer/Wrapper/CommandBuffer.hpp>
#include <Nazara/VulkanRenderer/Wrapper/CommandPool.hpp>
#include <Nazara/VulkanRenderer/Wrapper/QueueHandle.hpp>

#define VMA_IMPLEMENTATION
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#include <vma/vk_mem_alloc.h>

#include <Nazara/VulkanRenderer/Debug.hpp>

namespace Nz
{
	namespace Vk
	{
		struct Device::InternalData
		{
			Vk::CommandPool transferCommandPool;
		};

		Device::Device(Instance& instance) :
		m_instance(instance),
		m_physicalDevice(nullptr),
		m_device(VK_NULL_HANDLE),
		m_lastErrorCode(VK_SUCCESS),
		m_memAllocator(VK_NULL_HANDLE)
		{
		}

		Device::~Device()
		{
			if (m_device != VK_NULL_HANDLE)
				WaitAndDestroyDevice();
		}

		AutoCommandBuffer Device::AllocateTransferCommandBuffer()
		{
			return m_internalData->transferCommandPool.AllocateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY);
		}

		bool Device::Create(const Vk::PhysicalDevice& deviceInfo, const VkDeviceCreateInfo& createInfo, const VkAllocationCallbacks* allocator)
		{
			m_lastErrorCode = m_instance.vkCreateDevice(deviceInfo.physDevice, &createInfo, allocator, &m_device);
			if (m_lastErrorCode != VkResult::VK_SUCCESS)
			{
				NazaraError("Failed to create Vulkan device: " + TranslateVulkanError(m_lastErrorCode));
				return false;
			}

			CallOnExit destroyOnFailure([this] { Destroy(); });

			m_physicalDevice = &deviceInfo;

			// Store the allocator to access them when needed
			if (allocator)
				m_allocator = *allocator;
			else
				m_allocator.pfnAllocation = nullptr;

			// Parse extensions and layers
			for (UInt32 i = 0; i < createInfo.enabledExtensionCount; ++i)
				m_loadedExtensions.emplace(createInfo.ppEnabledExtensionNames[i]);

			for (UInt32 i = 0; i < createInfo.enabledLayerCount; ++i)
				m_loadedLayers.emplace(createInfo.ppEnabledLayerNames[i]);

			// Load all device-related functions
			try
			{
				ErrorFlags flags(ErrorFlag_ThrowException, true);

				UInt32 deviceVersion = deviceInfo.properties.apiVersion;

#define NAZARA_VULKANRENDERER_DEVICE_EXT_BEGIN(ext) if (IsExtensionLoaded(#ext)) {
#define NAZARA_VULKANRENDERER_DEVICE_EXT_END() }
#define NAZARA_VULKANRENDERER_DEVICE_FUNCTION(func) func = reinterpret_cast<PFN_##func>(GetProcAddr(#func));

#define NAZARA_VULKANRENDERER_INSTANCE_EXT_BEGIN(ext) if (m_instance.IsExtensionLoaded(#ext)) {
#define NAZARA_VULKANRENDERER_INSTANCE_EXT_END() }

#define NAZARA_VULKANRENDERER_DEVICE_CORE_EXT_FUNCTION(func, coreVersion, suffix, extName)   \
				if (deviceVersion >= coreVersion)                                            \
					func = reinterpret_cast<PFN_##func>(GetProcAddr(#func));                 \
				else if (IsExtensionLoaded("VK_" #suffix "_" #extName))                      \
					func = reinterpret_cast<PFN_##func##suffix>(GetProcAddr(#func #suffix));

#include <Nazara/VulkanRenderer/Wrapper/DeviceFunctions.hpp>

#undef NAZARA_VULKANRENDERER_DEVICE_CORE_EXT_FUNCTION
#undef NAZARA_VULKANRENDERER_DEVICE_EXT_BEGIN
#undef NAZARA_VULKANRENDERER_DEVICE_EXT_END
#undef NAZARA_VULKANRENDERER_DEVICE_FUNCTION
#undef NAZARA_VULKANRENDERER_INSTANCE_EXT_BEGIN
#undef NAZARA_VULKANRENDERER_INSTANCE_EXT_END
			}
			catch (const std::exception& e)
			{
				NazaraError(std::string("Failed to query device function: ") + e.what());
				return false;
			}

			// And retains informations about queues
			m_transferQueueFamilyIndex = UINT32_MAX;

			UInt32 maxFamilyIndex = 0;
			m_enabledQueuesInfos.resize(createInfo.queueCreateInfoCount);
			for (UInt32 i = 0; i < createInfo.queueCreateInfoCount; ++i)
			{
				const VkDeviceQueueCreateInfo& queueCreateInfo = createInfo.pQueueCreateInfos[i];
				QueueFamilyInfo& info = m_enabledQueuesInfos[i];

				info.familyIndex = queueCreateInfo.queueFamilyIndex;
				if (info.familyIndex > maxFamilyIndex)
					maxFamilyIndex = info.familyIndex;

				const VkQueueFamilyProperties& queueProperties = deviceInfo.queueFamilies[info.familyIndex];
				info.flags = queueProperties.queueFlags;
				info.minImageTransferGranularity = queueProperties.minImageTransferGranularity;
				info.timestampValidBits = queueProperties.timestampValidBits;

				info.queues.resize(queueCreateInfo.queueCount);
				for (UInt32 queueIndex = 0; queueIndex < queueCreateInfo.queueCount; ++queueIndex)
				{
					QueueInfo& queueInfo = info.queues[queueIndex];
					queueInfo.familyInfo = &info;
					queueInfo.priority = queueCreateInfo.pQueuePriorities[queueIndex];
					vkGetDeviceQueue(m_device, info.familyIndex, queueIndex, &queueInfo.queue);
				}

				if (info.flags & (VK_QUEUE_TRANSFER_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_GRAPHICS_BIT))
				{
					if (m_transferQueueFamilyIndex == UINT32_MAX)
						m_transferQueueFamilyIndex = info.familyIndex;
					else if ((info.flags & (VK_QUEUE_COMPUTE_BIT | VK_QUEUE_GRAPHICS_BIT)) == 0)
					{
						m_transferQueueFamilyIndex = info.familyIndex;
						break;
					}
				}
			}

			m_queuesByFamily.resize(maxFamilyIndex + 1);
			for (const QueueFamilyInfo& familyInfo : m_enabledQueuesInfos)
				m_queuesByFamily[familyInfo.familyIndex] = &familyInfo.queues;

			m_internalData = std::make_unique<InternalData>();
			if (!m_internalData->transferCommandPool.Create(*this, m_transferQueueFamilyIndex, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT))
			{
				NazaraError("Failed to create transfer command pool: " + TranslateVulkanError(m_internalData->transferCommandPool.GetLastErrorCode()));
				return false;
			}

			// Initialize VMA
			VmaVulkanFunctions vulkanFunctions = {
				m_instance.vkGetPhysicalDeviceProperties,
				m_instance.vkGetPhysicalDeviceMemoryProperties,
				vkAllocateMemory,
				vkFreeMemory,
				vkMapMemory,
				vkUnmapMemory,
				vkFlushMappedMemoryRanges,
				vkInvalidateMappedMemoryRanges,
				vkBindBufferMemory,
				vkBindImageMemory,
				vkGetBufferMemoryRequirements,
				vkGetImageMemoryRequirements,
				vkCreateBuffer,
				vkDestroyBuffer,
				vkCreateImage,
				vkDestroyImage,
				vkCmdCopyBuffer,
#if VMA_DEDICATED_ALLOCATION || VMA_VULKAN_VERSION >= 1001000
				vkGetBufferMemoryRequirements2,
				vkGetImageMemoryRequirements2,
#endif
#if VMA_BIND_MEMORY2 || VMA_VULKAN_VERSION >= 1001000
				vkBindBufferMemory2,
				vkBindImageMemory2,
#endif
#if VMA_MEMORY_BUDGET || VMA_VULKAN_VERSION >= 1001000
				m_instance.vkGetPhysicalDeviceMemoryProperties2,
#endif
			};

			VmaAllocatorCreateInfo allocatorInfo = {};
			allocatorInfo.physicalDevice = deviceInfo.physDevice;
			allocatorInfo.device = m_device;
			allocatorInfo.instance = m_instance;
			allocatorInfo.vulkanApiVersion = std::min<UInt32>(VK_API_VERSION_1_1, m_instance.GetApiVersion());
			allocatorInfo.pVulkanFunctions = &vulkanFunctions;

			if (vkGetBufferMemoryRequirements2 && vkGetImageMemoryRequirements2)
				allocatorInfo.flags |= VMA_ALLOCATOR_CREATE_KHR_DEDICATED_ALLOCATION_BIT;

			if (vkBindBufferMemory2 && vkBindImageMemory2)
				allocatorInfo.flags |= VMA_ALLOCATOR_CREATE_KHR_BIND_MEMORY2_BIT;

			if (IsExtensionLoaded(VK_EXT_MEMORY_BUDGET_EXTENSION_NAME))
				allocatorInfo.flags |= VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT;

			m_lastErrorCode = vmaCreateAllocator(&allocatorInfo, &m_memAllocator);
			if (m_lastErrorCode != VK_SUCCESS)
			{
				NazaraError("Failed to initialize Vulkan Memory Allocator (VMA): " + TranslateVulkanError(m_lastErrorCode));
				return false;
			}

			destroyOnFailure.Reset();

			return true;
		}

		void Device::Destroy()
		{
			if (m_device != VK_NULL_HANDLE)
			{
				WaitAndDestroyDevice();
				ResetPointers();
			}
		}

		QueueHandle Device::GetQueue(UInt32 queueFamilyIndex, UInt32 queueIndex)
		{
			const auto& queues = GetEnabledQueues(queueFamilyIndex);
			NazaraAssert(queueIndex < queues.size(), "Invalid queue index");

			return QueueHandle(*this, queues[queueIndex].queue);
		}

		void Device::ResetPointers()
		{
			m_device = VK_NULL_HANDLE;
			m_physicalDevice = nullptr;

			// Reset functions pointers
#define NAZARA_VULKANRENDERER_DEVICE_FUNCTION(func) func = nullptr;
#define NAZARA_VULKANRENDERER_DEVICE_CORE_EXT_FUNCTION(func, ...) NAZARA_VULKANRENDERER_DEVICE_FUNCTION(func)
#define NAZARA_VULKANRENDERER_DEVICE_EXT_BEGIN(ext)
#define NAZARA_VULKANRENDERER_DEVICE_EXT_END()
#define NAZARA_VULKANRENDERER_INSTANCE_EXT_BEGIN(ext)
#define NAZARA_VULKANRENDERER_INSTANCE_EXT_END()

#include <Nazara/VulkanRenderer/Wrapper/DeviceFunctions.hpp>

#undef NAZARA_VULKANRENDERER_DEVICE_CORE_EXT_FUNCTION
#undef NAZARA_VULKANRENDERER_DEVICE_FUNCTION
#undef NAZARA_VULKANRENDERER_DEVICE_EXT_BEGIN
#undef NAZARA_VULKANRENDERER_DEVICE_EXT_END
#undef NAZARA_VULKANRENDERER_INSTANCE_EXT_BEGIN
#undef NAZARA_VULKANRENDERER_INSTANCE_EXT_END
		}

		void Device::WaitAndDestroyDevice()
		{
			assert(m_device != VK_NULL_HANDLE);

			if (vkDeviceWaitIdle)
				vkDeviceWaitIdle(m_device);

			if (m_memAllocator != VK_NULL_HANDLE)
				vmaDestroyAllocator(m_memAllocator);

			m_internalData.reset();

			if (vkDestroyDevice)
				vkDestroyDevice(m_device, (m_allocator.pfnAllocation) ? &m_allocator : nullptr);
		}
	}
}