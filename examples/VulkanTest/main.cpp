#include <Nazara/Utility.hpp>
#include <Nazara/Renderer/Renderer.hpp>
#include <Nazara/Renderer/RenderBuffer.hpp>
#include <Nazara/Renderer/RenderWindow.hpp>
#include <Nazara/VulkanRenderer.hpp>
#include <array>
#include <iostream>

VKAPI_ATTR VkBool32 VKAPI_CALL MyDebugReportCallback(
	VkDebugUtilsMessageSeverityFlagBitsEXT           messageSeverity,
	VkDebugUtilsMessageTypeFlagsEXT                  messageTypes,
	const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
	void* pUserData)
{
	if (pCallbackData->messageIdNumber != 0)
		std::cerr << "#" << pCallbackData->messageIdNumber << " " << pCallbackData->messageIdNumber << ": ";

	std::cerr << pCallbackData->pMessage << std::endl;
	return VK_FALSE;
}

int main()
{
	Nz::ParameterList params;
	params.SetParameter("VkInstanceInfo_EnabledExtensionCount", 1LL);
	params.SetParameter("VkInstanceInfo_EnabledExtension0", "VK_EXT_debug_report");

	params.SetParameter("VkDeviceInfo_EnabledLayerCount", 1LL);
	params.SetParameter("VkDeviceInfo_EnabledLayer0", "VK_LAYER_LUNARG_standard_validation");
	params.SetParameter("VkInstanceInfo_EnabledLayerCount", 1LL);
	params.SetParameter("VkInstanceInfo_EnabledLayer0", "VK_LAYER_LUNARG_standard_validation");

	Nz::Renderer::SetParameters(params);

	Nz::Initializer<Nz::Renderer> loader;
	if (!loader)
	{
		std::cout << "Failed to initialize Vulkan" << std::endl;;
		return __LINE__;
	}

	Nz::VulkanRenderer* rendererImpl = static_cast<Nz::VulkanRenderer*>(Nz::Renderer::GetRendererImpl());

	Nz::Vk::Instance& instance = Nz::Vulkan::GetInstance();

	VkDebugUtilsMessengerCreateInfoEXT callbackCreateInfo = { VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT  };
	callbackCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT;
	callbackCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	callbackCreateInfo.pfnUserCallback = &MyDebugReportCallback;

	/* Register the callback */
	VkDebugUtilsMessengerEXT callback;

	instance.vkCreateDebugUtilsMessengerEXT(instance, &callbackCreateInfo, nullptr, &callback);


	std::vector<VkLayerProperties> layerProperties;
	if (!Nz::Vk::Loader::EnumerateInstanceLayerProperties(&layerProperties))
	{
		NazaraError("Failed to enumerate instance layer properties");
		return __LINE__;
	}

	for (const VkLayerProperties& properties : layerProperties)
	{
		std::cout << properties.layerName << ": \t" << properties.description << std::endl;
	}

	Nz::RenderWindow window;

	Nz::MeshParams meshParams;
	meshParams.matrix = Nz::Matrix4f::Rotate(Nz::EulerAnglesf(0.f, 90.f, 180.f)) * Nz::Matrix4f::Scale(Nz::Vector3f(0.002f));
	meshParams.vertexDeclaration = Nz::VertexDeclaration::Get(Nz::VertexLayout_XYZ_Normal_UV);

	Nz::String windowTitle = "Vulkan Test";
	if (!window.Create(Nz::VideoMode(800, 600, 32), windowTitle))
	{
		std::cout << "Failed to create Window" << std::endl;
		return __LINE__;
	}

	std::shared_ptr<Nz::RenderDevice> device = window.GetRenderDevice();

	auto fragmentShader = device->InstantiateShaderStage(Nz::ShaderStageType::Fragment, Nz::ShaderLanguage::SpirV, "resources/shaders/triangle.frag.spv");
	if (!fragmentShader)
	{
		std::cout << "Failed to instantiate fragment shader" << std::endl;
		return __LINE__;
	}

	auto vertexShader = device->InstantiateShaderStage(Nz::ShaderStageType::Vertex, Nz::ShaderLanguage::SpirV, "resources/shaders/triangle.vert.spv");
	if (!vertexShader)
	{
		std::cout << "Failed to instantiate fragment shader" << std::endl;
		return __LINE__;
	}

	Nz::MeshRef drfreak = Nz::Mesh::LoadFromFile("resources/Spaceship/spaceship.obj", meshParams);

	if (!drfreak)
	{
		NazaraError("Failed to load model");
		return __LINE__;
	}

	Nz::StaticMesh* drfreakMesh = static_cast<Nz::StaticMesh*>(drfreak->GetSubMesh(0));

	const Nz::VertexBuffer* drfreakVB = drfreakMesh->GetVertexBuffer();
	const Nz::IndexBuffer* drfreakIB = drfreakMesh->GetIndexBuffer();

	// Index buffer
	std::cout << "Index count: " << drfreakIB->GetIndexCount() << std::endl;

	// Vertex buffer
	std::cout << "Vertex count: " << drfreakVB->GetVertexCount() << std::endl;

	Nz::VkRenderWindow& vulkanWindow = *static_cast<Nz::VkRenderWindow*>(window.GetImpl());
	Nz::VulkanDevice& vulkanDevice = vulkanWindow.GetDevice();

	Nz::Vk::CommandPool cmdPool;
	if (!cmdPool.Create(vulkanDevice, 0, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT))
	{
		NazaraError("Failed to create rendering cmd pool");
		return __LINE__;
	}

	Nz::Vk::QueueHandle graphicsQueue = vulkanDevice.GetQueue(0, 0);

	// Texture
	Nz::ImageRef drfreakImage = Nz::Image::LoadFromFile("resources/Spaceship/Texture/diffuse.png");
	if (!drfreakImage || !drfreakImage->Convert(Nz::PixelFormatType_RGBA8))
	{
		NazaraError("Failed to load image");
		return __LINE__;
	}

	Nz::TextureInfo texParams;
	texParams.pixelFormat = drfreakImage->GetFormat();
	texParams.type = drfreakImage->GetType();
	texParams.width = drfreakImage->GetWidth();
	texParams.height = drfreakImage->GetHeight();
	texParams.depth = drfreakImage->GetDepth();

	std::unique_ptr<Nz::Texture> texture = device->InstantiateTexture(texParams);
	if (!texture->Update(drfreakImage->GetConstPixels()))
	{
		NazaraError("Failed to update texture");
		return __LINE__;
	}

	std::unique_ptr<Nz::TextureSampler> textureSampler = device->InstantiateTextureSampler({});

	struct
	{
		Nz::Matrix4f projectionMatrix;
		Nz::Matrix4f modelMatrix;
		Nz::Matrix4f viewMatrix;
	}
	ubo;

	Nz::Vector2ui windowSize = window.GetSize();
	ubo.projectionMatrix = Nz::Matrix4f::Perspective(70.f, float(windowSize.x) / windowSize.y, 0.1f, 1000.f);
	ubo.viewMatrix = Nz::Matrix4f::Translate(Nz::Vector3f::Backward() * 1);
	ubo.modelMatrix = Nz::Matrix4f::Translate(Nz::Vector3f::Forward() * 2 + Nz::Vector3f::Right());

	Nz::UInt32 uniformSize = sizeof(ubo);

	Nz::RenderPipelineLayoutInfo pipelineLayoutInfo;
	auto& uboBinding = pipelineLayoutInfo.bindings.emplace_back();
	uboBinding.index = 0;
	uboBinding.shaderStageFlags = Nz::ShaderStageType::Vertex;
	uboBinding.type = Nz::ShaderBindingType::UniformBuffer;

	auto& textureBinding = pipelineLayoutInfo.bindings.emplace_back();
	textureBinding.index = 1;
	textureBinding.shaderStageFlags = Nz::ShaderStageType::Fragment;
	textureBinding.type = Nz::ShaderBindingType::Texture;

	std::shared_ptr<Nz::RenderPipelineLayout> renderPipelineLayout = device->InstantiateRenderPipelineLayout(pipelineLayoutInfo);

	Nz::ShaderBinding& shaderBinding = renderPipelineLayout->AllocateShaderBinding();

	std::unique_ptr<Nz::AbstractBuffer> uniformBuffer = device->InstantiateBuffer(Nz::BufferType_Uniform);
	if (!uniformBuffer->Initialize(uniformSize, Nz::BufferUsage_DeviceLocal))
	{
		NazaraError("Failed to create uniform buffer");
		return __LINE__;
	}

	shaderBinding.Update({
		{
			0,
			Nz::ShaderBinding::UniformBufferBinding {
				uniformBuffer.get(), 0, uniformSize
			}
		},
		{
			1,
			Nz::ShaderBinding::TextureBinding {
				texture.get(), textureSampler.get()
			}
		}
	});

	Nz::RenderPipelineInfo pipelineInfo;
	pipelineInfo.pipelineLayout = renderPipelineLayout;

	pipelineInfo.depthBuffer = true;
	pipelineInfo.shaderStages.emplace_back(fragmentShader);
	pipelineInfo.shaderStages.emplace_back(vertexShader);

	auto& vertexBuffer = pipelineInfo.vertexBuffers.emplace_back();
	vertexBuffer.binding = 0;
	vertexBuffer.declaration = drfreakVB->GetVertexDeclaration();

	std::unique_ptr<Nz::RenderPipeline> pipeline = device->InstantiateRenderPipeline(pipelineInfo);

	std::array<VkClearValue, 2> clearValues;
	clearValues[0].color = {0.0f, 0.0f, 0.0f, 0.0f};
	clearValues[1].depthStencil = {1.f, 0};

	Nz::UInt32 imageCount = vulkanWindow.GetFramebufferCount();
	std::vector<Nz::Vk::CommandBuffer> renderCmds = cmdPool.AllocateCommandBuffers(imageCount, VK_COMMAND_BUFFER_LEVEL_PRIMARY);

	Nz::RenderBuffer* renderBufferIB = static_cast<Nz::RenderBuffer*>(drfreakIB->GetBuffer()->GetImpl());
	Nz::RenderBuffer* renderBufferVB = static_cast<Nz::RenderBuffer*>(drfreakVB->GetBuffer()->GetImpl());

	if (!renderBufferIB->Synchronize(&vulkanDevice))
	{
		NazaraError("Failed to synchronize render buffer");
		return __LINE__;
	}

	if (!renderBufferVB->Synchronize(&vulkanDevice))
	{
		NazaraError("Failed to synchronize render buffer");
		return __LINE__;
	}

	Nz::VulkanBuffer* indexBufferImpl = static_cast<Nz::VulkanBuffer*>(renderBufferIB->GetHardwareBuffer(&vulkanDevice));
	Nz::VulkanBuffer* vertexBufferImpl = static_cast<Nz::VulkanBuffer*>(renderBufferVB->GetHardwareBuffer(&vulkanDevice));

	Nz::VulkanRenderPipeline* vkPipeline = static_cast<Nz::VulkanRenderPipeline*>(pipeline.get());

	Nz::VulkanRenderPipelineLayout* vkPipelineLayout = static_cast<Nz::VulkanRenderPipelineLayout*>(renderPipelineLayout.get());

	Nz::VulkanShaderBinding& vkShaderBinding = static_cast<Nz::VulkanShaderBinding&>(shaderBinding);

	for (Nz::UInt32 i = 0; i < imageCount; ++i)
	{
		Nz::Vk::CommandBuffer& renderCmd = renderCmds[i];

		VkRect2D renderArea = {
			{                                           // VkOffset2D                     offset
				0,                                          // int32_t                        x
				0                                           // int32_t                        y
			},
			{                                           // VkExtent2D                     extent
				windowSize.x,                                        // int32_t                        width
				windowSize.y,                                        // int32_t                        height
			}
		};

		VkRenderPassBeginInfo render_pass_begin_info = {
			VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,     // VkStructureType                sType
			nullptr,                                      // const void                    *pNext
			vulkanWindow.GetRenderPass(),                            // VkRenderPass                   renderPass
			vulkanWindow.GetFrameBuffer(i),                       // VkFramebuffer                  framebuffer
			renderArea,
			2U,                                            // uint32_t                       clearValueCount
			clearValues.data()                                  // const VkClearValue            *pClearValues
		};

		renderCmd.Begin();
		renderCmd.BeginDebugRegion("Main window rendering", Nz::Color::Green);
		renderCmd.BeginRenderPass(render_pass_begin_info);
		renderCmd.BindIndexBuffer(indexBufferImpl->GetBuffer(), 0, VK_INDEX_TYPE_UINT16);
		renderCmd.BindVertexBuffer(0, vertexBufferImpl->GetBuffer(), 0);
		renderCmd.BindDescriptorSet(VK_PIPELINE_BIND_POINT_GRAPHICS, vkPipelineLayout->GetPipelineLayout(), 0, vkShaderBinding.GetDescriptorSet());
		renderCmd.BindPipeline(VK_PIPELINE_BIND_POINT_GRAPHICS, vkPipeline->Get(vulkanWindow.GetRenderPass()));
		renderCmd.SetScissor(Nz::Recti{0, 0, int(windowSize.x), int(windowSize.y)});
		renderCmd.SetViewport({0.f, 0.f, float(windowSize.x), float(windowSize.y)}, 0.f, 1.f);
		renderCmd.DrawIndexed(drfreakIB->GetIndexCount());
		renderCmd.EndRenderPass();
		renderCmd.EndDebugRegion();

		if (!renderCmd.End())
		{
			NazaraError("Failed to specify render cmd");
			return __LINE__;
		}
	}

	Nz::Vector3f viewerPos = Nz::Vector3f::Zero();

	Nz::EulerAnglesf camAngles(0.f, 0.f, 0.f);
	Nz::Quaternionf camQuat(camAngles);

	window.EnableEventPolling(true);

	struct ImageData
	{
		Nz::Vk::Fence inflightFence;
		Nz::Vk::Semaphore imageAvailableSemaphore;
		Nz::Vk::Semaphore renderFinishedSemaphore;
		Nz::Vk::AutoCommandBuffer commandBuffer;
		std::optional<Nz::VulkanUploadPool> uploadPool;
	};

	const std::size_t MaxConcurrentImage = imageCount;

	Nz::Vk::CommandPool transientPool;
	transientPool.Create(vulkanDevice, 0, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
	transientPool.SetDebugName("Transient command pool");

	std::vector<ImageData> frameSync(MaxConcurrentImage);
	for (ImageData& syncData : frameSync)
	{
		syncData.imageAvailableSemaphore.Create(vulkanDevice);
		syncData.renderFinishedSemaphore.Create(vulkanDevice);

		syncData.inflightFence.Create(vulkanDevice, VK_FENCE_CREATE_SIGNALED_BIT);

		syncData.uploadPool.emplace(vulkanDevice, 8 * 1024 * 1024);

		syncData.commandBuffer = transientPool.AllocateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY);
	}

	std::vector<Nz::Vk::Fence*> inflightFences(imageCount, nullptr);

	std::size_t currentFrame = 0;

	Nz::Clock updateClock;
	Nz::Clock secondClock;
	unsigned int fps = 0;

	while (window.IsOpen())
	{
		Nz::WindowEvent event;
		while (window.PollEvent(&event))
		{
			switch (event.type)
			{
				case Nz::WindowEventType_Quit:
					window.Close();
					break;

				case Nz::WindowEventType_MouseMoved: // La souris a bougé
				{
					// Gestion de la caméra free-fly (Rotation)
					float sensitivity = 0.3f; // Sensibilité de la souris

					// On modifie l'angle de la caméra grâce au déplacement relatif sur X de la souris
					camAngles.yaw = Nz::NormalizeAngle(camAngles.yaw - event.mouseMove.deltaX*sensitivity);

					// Idem, mais pour éviter les problèmes de calcul de la matrice de vue, on restreint les angles
					camAngles.pitch = Nz::Clamp(camAngles.pitch + event.mouseMove.deltaY*sensitivity, -89.f, 89.f);

					camQuat = camAngles;

					// Pour éviter que le curseur ne sorte de l'écran, nous le renvoyons au centre de la fenêtre
					// Cette fonction est codée de sorte à ne pas provoquer d'évènement MouseMoved
					Nz::Mouse::SetPosition(windowSize.x / 2, windowSize.y / 2, window);
					break;
				}
			}
		}

		if (updateClock.GetMilliseconds() > 1000 / 60)
		{
			float cameraSpeed = 2.f * updateClock.GetSeconds();
			updateClock.Restart();

			if (Nz::Keyboard::IsKeyPressed(Nz::Keyboard::Up) || Nz::Keyboard::IsKeyPressed(Nz::Keyboard::Z))
				viewerPos += camQuat * Nz::Vector3f::Forward() * cameraSpeed;

			// Si la flèche du bas ou la touche S est pressée, on recule
			if (Nz::Keyboard::IsKeyPressed(Nz::Keyboard::Down) || Nz::Keyboard::IsKeyPressed(Nz::Keyboard::S))
				viewerPos += camQuat * Nz::Vector3f::Backward() * cameraSpeed;

			// Etc...
			if (Nz::Keyboard::IsKeyPressed(Nz::Keyboard::Left) || Nz::Keyboard::IsKeyPressed(Nz::Keyboard::Q))
				viewerPos += camQuat * Nz::Vector3f::Left() * cameraSpeed;

			// Etc...
			if (Nz::Keyboard::IsKeyPressed(Nz::Keyboard::Right) || Nz::Keyboard::IsKeyPressed(Nz::Keyboard::D))
				viewerPos += camQuat * Nz::Vector3f::Right() * cameraSpeed;

			// Majuscule pour monter, notez l'utilisation d'une direction globale (Non-affectée par la rotation)
			if (Nz::Keyboard::IsKeyPressed(Nz::Keyboard::LShift) || Nz::Keyboard::IsKeyPressed(Nz::Keyboard::RShift))
				viewerPos += Nz::Vector3f::Up() * cameraSpeed;

			// Contrôle (Gauche ou droite) pour descendre dans l'espace global, etc...
			if (Nz::Keyboard::IsKeyPressed(Nz::Keyboard::LControl) || Nz::Keyboard::IsKeyPressed(Nz::Keyboard::RControl))
				viewerPos += Nz::Vector3f::Down() * cameraSpeed;
		}

		ImageData& frame = frameSync[currentFrame];
		frame.inflightFence.Wait();

		Nz::UInt32 imageIndex;
		if (!vulkanWindow.Acquire(&imageIndex, frame.imageAvailableSemaphore))
		{
			std::cout << "Failed to acquire next image" << std::endl;
			return EXIT_FAILURE;
		}

		if (inflightFences[imageIndex])
			inflightFences[imageIndex]->Wait();

		inflightFences[imageIndex] = &frame.inflightFence;
		inflightFences[imageIndex]->Reset();

		// Update UBO
		frame.uploadPool->Reset();

		ubo.viewMatrix = Nz::Matrix4f::ViewMatrix(viewerPos, camAngles);

		auto allocData = frame.uploadPool->Allocate(uniformSize);
		assert(allocData);

		std::memcpy(allocData->mappedPtr, &ubo, sizeof(ubo));

		frame.commandBuffer->Begin(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
		frame.commandBuffer->BeginDebugRegion("UBO Update", Nz::Color::Yellow);
		frame.commandBuffer->MemoryBarrier(VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0U, VK_ACCESS_TRANSFER_READ_BIT);
		frame.commandBuffer->CopyBuffer(allocData->buffer, static_cast<Nz::VulkanBuffer*>(uniformBuffer.get())->GetBuffer(), allocData->size, allocData->offset);
		frame.commandBuffer->MemoryBarrier(VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_UNIFORM_READ_BIT);
		frame.commandBuffer->EndDebugRegion();
		frame.commandBuffer->End();

		if (!graphicsQueue.Submit(frame.commandBuffer))
			return false;

		if (!graphicsQueue.Submit(renderCmds[imageIndex], frame.imageAvailableSemaphore, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, frame.renderFinishedSemaphore, frame.inflightFence))
			return false;

		vulkanWindow.Present(imageIndex, frame.renderFinishedSemaphore);

		// On incrémente le compteur de FPS improvisé
		fps++;

		if (secondClock.GetMilliseconds() >= 1000) // Toutes les secondes
		{
			// Et on insère ces données dans le titre de la fenêtre
			window.SetTitle(windowTitle + " - " + Nz::String::Number(fps) + " FPS");

			/*
			Note: En C++11 il est possible d'insérer de l'Unicode de façon standard, quel que soit l'encodage du fichier,
			via quelque chose de similaire à u8"Cha\u00CEne de caract\u00E8res".
			Cependant, si le code source est encodé en UTF-8 (Comme c'est le cas dans ce fichier),
			cela fonctionnera aussi comme ceci : "Chaîne de caractères".
			*/

			// Et on réinitialise le compteur de FPS
			fps = 0;

			// Et on relance l'horloge pour refaire ça dans une seconde
			secondClock.Restart();
		}

		currentFrame = (currentFrame + 1) % imageCount;
	}

	instance.vkDestroyDebugUtilsMessengerEXT(instance, callback, nullptr);

	return EXIT_SUCCESS;
}