#include "application.hpp"

#include <vulkan/vulkan.h>
#include <imgui.h>
#include <fstream>
#include <vector>
#include <iostream>
#include <cstring>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace application {

namespace {

VkShaderModule vk_shader_module_vert;
VkShaderModule vk_shader_module_frag;
VkPipeline vk_pipeline;
VkPipelineLayout vk_pipeline_layout;
VkBuffer vk_vertex_buffer;
VmaAllocation vma_vertex_buffer_allocation;

VkBuffer vk_index_buffer;
VmaAllocation vma_index_buffer_allocation;

uint32_t index_count;

struct Vertex {
	float position[3];
	float color[3];
};

const Vertex cube_vertices[] = {
	{ { -0.3f, -0.3f, -0.3f }, { 1.0f, 0.0f, 0.0f } },
	{ {  0.3f, -0.3f, -0.3f }, { 0.0f, 1.0f, 0.0f } },
	{ {  0.3f,  0.3f, -0.3f }, { 0.0f, 0.0f, 1.0f } },
	{ { -0.3f,  0.3f, -0.3f }, { 1.0f, 1.0f, 0.0f } },
	{ { -0.3f, -0.3f,  0.3f }, { 1.0f, 0.0f, 1.0f } },
	{ {  0.3f, -0.3f,  0.3f }, { 0.0f, 1.0f, 1.0f } },
	{ {  0.3f,  0.3f,  0.3f }, { 1.0f, 1.0f, 1.0f } },
	{ { -0.3f,  0.3f,  0.3f }, { 0.5f, 0.5f, 0.5f } },
};

const uint32_t cube_indices[] = {
	// задняя 
	0, 1, 2,   0, 2, 3,
	// передняя 
	4, 6, 5,   4, 7, 6,
	// левая 
	0, 3, 7,   0, 7, 4,
	// правая 
	1, 5, 6,   1, 6, 2,
	// нижняя
	0, 4, 5,   0, 5, 1,
	// верхняя 
	3, 2, 6,   3, 6, 7,
};

struct UniformBufferObject {
	glm::mat4 model;
	glm::mat4 view;
	glm::mat4 proj;
};

VkBuffer vk_uniform_buffer;
VmaAllocation vma_uniform_buffer_allocation;
VmaAllocationInfo vma_uniform_buffer_info;

VkDescriptorSetLayout vk_descriptor_set_layout;
VkDescriptorPool vk_descriptor_pool;
VkDescriptorSet vk_descriptor_set;

bool use_orthographic = false;

float cube_position[3] = { 0.0f, 0.0f, 0.0f }; //проекция
float cube_rotation[3] = { 0.0f, 0.0f, 0.0f }; //поворот в градусах
float cube_scale[3]    = { 1.0f, 1.0f, 1.0f }; //масштаб

} // namespace

std::vector<char> readFile(const std::string& filename) {
	std::ifstream file(filename, std::ios::ate | std::ios::binary);

	if (!file.is_open()) {
		std::cerr << "Failed to open file: " << filename << '\n';
		return {};
	}

	const size_t file_size = size_t(file.tellg());
	std::vector<char> buffer(file_size);

	file.seekg(0);
	file.read(buffer.data(), std::streamsize(file_size));

	file.close();

	return buffer;
}

VkShaderModule createShaderModule(const std::vector<char>& code) {
	const VkShaderModuleCreateInfo create_info = {
		.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
		.codeSize = code.size(),
		.pCode = reinterpret_cast<const uint32_t*>(code.data()),
	};

	VkShaderModule shader_module;
	if (vkCreateShaderModule(graphics::internal::context.device, &create_info, nullptr, &shader_module) != VK_SUCCESS) {
		std::cerr << "Failed to create shader module\n";
		return VK_NULL_HANDLE;
	}

	return shader_module;
}

bool createGraphicsPipeline() {
	
	const VkPipelineShaderStageCreateInfo shader_stages[] = {
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_VERTEX_BIT,
			.module = vk_shader_module_vert,
			.pName = "main",  // имя функции в шейдере
		},
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
			.module = vk_shader_module_frag,
			.pName = "main",
		},
	};

	const VkVertexInputBindingDescription vertex_binding = {
		.binding = 0,                              // номер привязки
		.stride = sizeof(float) * 6,               // размер одной вершины в байтах (6 float)
		.inputRate = VK_VERTEX_INPUT_RATE_VERTEX,  // данные меняются на каждую вершину
	};

	const VkVertexInputAttributeDescription vertex_attributes[] = {
		{
			.location = 0,                     // location = 0 в шейдере
			.binding = 0,
			.format = VK_FORMAT_R32G32B32_SFLOAT,  // vec3
			.offset = 0,                       // смещение в структуре вершины
		},
		{
			.location = 1,                     // location = 1 в шейдере
			.binding = 0,
			.format = VK_FORMAT_R32G32B32_SFLOAT,  // vec3
			.offset = sizeof(float) * 3,       // смещение после position
		},
	};

	const VkPipelineVertexInputStateCreateInfo vertex_input = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		.vertexBindingDescriptionCount = 1,
		.pVertexBindingDescriptions = &vertex_binding,
		.vertexAttributeDescriptionCount = 2,
		.pVertexAttributeDescriptions = vertex_attributes,
	};

	const VkPipelineInputAssemblyStateCreateInfo input_assembly = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
		.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,  // треугольники
		.primitiveRestartEnable = VK_FALSE,
	};

	VkViewport viewport = {
		.x = 0.0f,
		.y = 0.0f,
		.width = float(graphics::internal::context.swapchain_extent.width),
		.height = float(graphics::internal::context.swapchain_extent.height),
		.minDepth = 0.0f,
		.maxDepth = 1.0f,
	};

	VkRect2D scissor = {
		.offset = { 0, 0 },
		.extent = graphics::internal::context.swapchain_extent,
	};

	const VkPipelineViewportStateCreateInfo viewport_state = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
		.viewportCount = 1,
		.pViewports = &viewport,
		.scissorCount = 1,
		.pScissors = &scissor,
	};

	const VkPipelineRasterizationStateCreateInfo rasterization = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
		.depthClampEnable = VK_FALSE,           // не отсекать по глубине
		.rasterizerDiscardEnable = VK_FALSE,    // не отбрасывать всё
		.polygonMode = VK_POLYGON_MODE_FILL,    // заливать треугольники
		.cullMode = VK_CULL_MODE_BACK_BIT,      // отбрасывать задние грани
		.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,  // передние грани — против часовой
		.depthBiasEnable = VK_FALSE,
		.lineWidth = 1.0f,
	};

	const VkPipelineMultisampleStateCreateInfo multisampling = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
		.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
		.sampleShadingEnable = VK_FALSE,
	};

	const VkPipelineDepthStencilStateCreateInfo depth_stencil = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
		.depthTestEnable = VK_TRUE,             // включить тест глубины
		.depthWriteEnable = VK_TRUE,            // записывать глубину
		.depthCompareOp = VK_COMPARE_OP_LESS,   // рисовать, если ближе
		.depthBoundsTestEnable = VK_FALSE,
		.stencilTestEnable = VK_FALSE,
	};

	const VkPipelineColorBlendAttachmentState color_blend_attachment = {
		.blendEnable = VK_FALSE,                // без смешивания
		.colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
		                  VK_COLOR_COMPONENT_G_BIT |
		                  VK_COLOR_COMPONENT_B_BIT |
		                  VK_COLOR_COMPONENT_A_BIT,
	};

	const VkPipelineColorBlendStateCreateInfo color_blending = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
		.logicOpEnable = VK_FALSE,
		.attachmentCount = 1,
		.pAttachments = &color_blend_attachment,
	};

	const VkPipelineLayoutCreateInfo pipeline_layout_info = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount = 1,
		.pSetLayouts = &vk_descriptor_set_layout,
		.pushConstantRangeCount = 0,
		.pPushConstantRanges = nullptr,
	};

	if (vkCreatePipelineLayout(graphics::internal::context.device, &pipeline_layout_info, nullptr, &vk_pipeline_layout) != VK_SUCCESS) {
		std::cerr << "Failed to create pipeline layout\n";
		return false;
	}

	const VkGraphicsPipelineCreateInfo pipeline_info = {
		.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
		.stageCount = 2,
		.pStages = shader_stages,
		.pVertexInputState = &vertex_input,
		.pInputAssemblyState = &input_assembly,
		.pViewportState = &viewport_state,
		.pRasterizationState = &rasterization,
		.pMultisampleState = &multisampling,
		.pDepthStencilState = &depth_stencil,
		.pColorBlendState = &color_blending,
		.layout = vk_pipeline_layout,
		.renderPass = graphics::internal::context.render_pass,
		.subpass = 0,
	};

	if (vkCreateGraphicsPipelines(graphics::internal::context.device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &vk_pipeline) != VK_SUCCESS) {
		std::cerr << "Failed to create graphics pipeline\n";
		return false;
	}

	std::cout << "Graphics pipeline created successfully\n";
	return true;
}

bool createVertexBuffer() {
	const VkDeviceSize buffer_size = sizeof(cube_vertices);

	const VkBufferCreateInfo buffer_info = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = buffer_size,
		.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,  // это vertex buffer
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
	};

	const VmaAllocationCreateInfo allocation_info = {
		.usage = VMA_MEMORY_USAGE_AUTO,
		.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
		         VMA_ALLOCATION_CREATE_MAPPED_BIT,
	};

	VmaAllocationInfo allocation_result;
	if (vmaCreateBuffer(graphics::internal::context.allocator, &buffer_info, &allocation_info,
	                    &vk_vertex_buffer, &vma_vertex_buffer_allocation,
	                    &allocation_result) != VK_SUCCESS) {
		std::cerr << "Failed to create vertex buffer\n";
		return false;
	}

	// Копируем данные вершин в память GPU
	memcpy(allocation_result.pMappedData, cube_vertices, buffer_size);

	std::cout << "Vertex buffer created successfully\n";
	return true;
}

bool createIndexBuffer() {
	index_count = sizeof(cube_indices) / sizeof(cube_indices[0]);
	const VkDeviceSize buffer_size = sizeof(cube_indices);

	const VkBufferCreateInfo buffer_info = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = buffer_size,
		.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT,  // это index buffer
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
	};

	const VmaAllocationCreateInfo allocation_info = {
		.usage = VMA_MEMORY_USAGE_AUTO,
		.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
		         VMA_ALLOCATION_CREATE_MAPPED_BIT,
	};

	VmaAllocationInfo allocation_result;
	if (vmaCreateBuffer(graphics::internal::context.allocator, &buffer_info, &allocation_info,
	                    &vk_index_buffer, &vma_index_buffer_allocation,
	                    &allocation_result) != VK_SUCCESS) {
		std::cerr << "Failed to create index buffer\n";
		return false;
	}

	memcpy(allocation_result.pMappedData, cube_indices, buffer_size);

	std::cout << "Index buffer created successfully\n";
	return true;
}

bool createUniformBuffer() {
	const VkDeviceSize buffer_size = sizeof(UniformBufferObject);

	const VkBufferCreateInfo buffer_info = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = buffer_size,
		.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
	};

	const VmaAllocationCreateInfo allocation_info = {
		.usage = VMA_MEMORY_USAGE_AUTO,
		.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
		         VMA_ALLOCATION_CREATE_MAPPED_BIT,
	};

	if (vmaCreateBuffer(graphics::internal::context.allocator, &buffer_info, &allocation_info,
	                    &vk_uniform_buffer, &vma_uniform_buffer_allocation,
	                    &vma_uniform_buffer_info) != VK_SUCCESS) {
		std::cerr << "Failed to create uniform buffer\n";
		return false;
	}

	std::cout << "Uniform buffer created successfully\n";
	return true;
}

bool createDescriptorSetLayout() {
	const VkDescriptorSetLayoutBinding ubo_layout_binding = {
		.binding = 0,                                        // binding = 0 в шейдере
		.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, // это uniform buffer
		.descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_VERTEX_BIT,            // используется в vertex shader
		.pImmutableSamplers = nullptr,
	};

	const VkDescriptorSetLayoutCreateInfo layout_info = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.bindingCount = 1,
		.pBindings = &ubo_layout_binding,
	};

	if (vkCreateDescriptorSetLayout(graphics::internal::context.device, &layout_info, nullptr, &vk_descriptor_set_layout) != VK_SUCCESS) {
		std::cerr << "Failed to create descriptor set layout\n";
		return false;
	}

	std::cout << "Descriptor set layout created successfully\n";
	return true;
}

bool createDescriptorPool() {
	const VkDescriptorPoolSize pool_size = {
		.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		.descriptorCount = 1,
	};

	const VkDescriptorPoolCreateInfo pool_info = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.poolSizeCount = 1,
		.pPoolSizes = &pool_size,
		.maxSets = 1,
	};

	if (vkCreateDescriptorPool(graphics::internal::context.device, &pool_info, nullptr, &vk_descriptor_pool) != VK_SUCCESS) {
		std::cerr << "Failed to create descriptor pool\n";
		return false;
	}

	std::cout << "Descriptor pool created successfully\n";
	return true;
}

bool createDescriptorSet() {
	const VkDescriptorSetAllocateInfo alloc_info = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
		.descriptorPool = vk_descriptor_pool,
		.descriptorSetCount = 1,
		.pSetLayouts = &vk_descriptor_set_layout,
	};

	if (vkAllocateDescriptorSets(graphics::internal::context.device, &alloc_info, &vk_descriptor_set) != VK_SUCCESS) {
		std::cerr << "Failed to allocate descriptor set\n";
		return false;
	}

	const VkDescriptorBufferInfo buffer_info = {
		.buffer = vk_uniform_buffer,
		.offset = 0,
		.range = sizeof(UniformBufferObject),
	};

	const VkWriteDescriptorSet descriptor_write = {
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
		.dstSet = vk_descriptor_set,
		.dstBinding = 0,
		.dstArrayElement = 0,
		.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		.descriptorCount = 1,
		.pBufferInfo = &buffer_info,
	};

	vkUpdateDescriptorSets(graphics::internal::context.device, 1, &descriptor_write, 0, nullptr);

	std::cout << "Descriptor set created successfully\n";
	return true;
}

bool initialize() {
	const std::vector<char> vert_code = readFile("shaders/cube.vert.spv");
	if (vert_code.empty()) {
		std::cerr << "Failed to read vertex shader\n";
		return false;
	}

	const std::vector<char> frag_code = readFile("shaders/cube.frag.spv");
	if (frag_code.empty()) {
		std::cerr << "Failed to read fragment shader\n";
		return false;
	}

	vk_shader_module_vert = createShaderModule(vert_code);
	if (vk_shader_module_vert == VK_NULL_HANDLE) {
		std::cerr << "Failed to create vertex shader module\n";
		return false;
	}

	vk_shader_module_frag = createShaderModule(frag_code);
	if (vk_shader_module_frag == VK_NULL_HANDLE) {
		std::cerr << "Failed to create fragment shader module\n";
		return false;
	}

	if (!createDescriptorSetLayout()) {
		std::cerr << "Failed to create descriptor set layout\n";
		return false;
	}

	if (!createGraphicsPipeline()) {
		std::cerr << "Failed to create graphics pipeline\n";
		return false;
	}

	vkDestroyShaderModule(graphics::internal::context.device, vk_shader_module_vert, nullptr);
	vkDestroyShaderModule(graphics::internal::context.device, vk_shader_module_frag, nullptr);
	vk_shader_module_vert = VK_NULL_HANDLE;
	vk_shader_module_frag = VK_NULL_HANDLE;

	if (!createVertexBuffer()) {
		std::cerr << "Failed to create vertex buffer\n";
		return false;
	}

	if (!createIndexBuffer()) {
		std::cerr << "Failed to create index buffer\n";
		return false;
	}

	if (!createUniformBuffer()) {
		std::cerr << "Failed to create uniform buffer\n";
		return false;
	}

	if (!createDescriptorPool()) {
		std::cerr << "Failed to create descriptor pool\n";
		return false;
	}

	if (!createDescriptorSet()) {
		std::cerr << "Failed to create descriptor set\n";
		return false;
	}

	std::cout << "Application initialized successfully\n";
	return true;
}

void shutdown() {
	auto& context = graphics::internal::context;
	vkQueueWaitIdle(context.graphics_queue);

	if (vk_shader_module_vert != VK_NULL_HANDLE) {
		vkDestroyShaderModule(context.device, vk_shader_module_vert, nullptr);
	}
	if (vk_shader_module_frag != VK_NULL_HANDLE) {
		vkDestroyShaderModule(context.device, vk_shader_module_frag, nullptr);
	}

	vkDestroyPipeline(context.device, vk_pipeline, nullptr);
	vkDestroyPipelineLayout(context.device, vk_pipeline_layout, nullptr);

	vmaDestroyBuffer(context.allocator, vk_vertex_buffer, vma_vertex_buffer_allocation);
	vmaDestroyBuffer(context.allocator, vk_index_buffer, vma_index_buffer_allocation);

		vkDestroyDescriptorPool(context.device, vk_descriptor_pool, nullptr);
	vkDestroyDescriptorSetLayout(context.device, vk_descriptor_set_layout, nullptr);
	vmaDestroyBuffer(context.allocator, vk_uniform_buffer, vma_uniform_buffer_allocation);
}

void update([[maybe_unused]] double time) {
	ImGui::ShowDemoWindow();

	ImGui::Begin("Controls");
	ImGui::Checkbox("Orthographic projection", &use_orthographic);
	ImGui::SetNextItemWidth(400.0f);
	ImGui::SliderFloat3("Position", cube_position, -3.0f, 3.0f);

	ImGui::SetNextItemWidth(400.0f);
	ImGui::SliderFloat3("Rotation", cube_rotation, -180.0f, 180.0f);

	ImGui::SetNextItemWidth(400.0f);
	ImGui::SliderFloat3("Scale", cube_scale, 0.1f, 3.0f);
	ImGui::End();

	UniformBufferObject ubo{};

	// Model: вращаем куб вокруг оси Y
	glm::mat4 model = glm::mat4(1.0f);

	// Позиция
	model = glm::translate(model, glm::vec3(cube_position[0], cube_position[1], cube_position[2]));

	// Поворот (по осям X, Y, Z)
	model = glm::rotate(model, glm::radians(cube_rotation[0]), glm::vec3(1.0f, 0.0f, 0.0f));
	model = glm::rotate(model, glm::radians(cube_rotation[1]), glm::vec3(0.0f, 1.0f, 0.0f));
	model = glm::rotate(model, glm::radians(cube_rotation[2]), glm::vec3(0.0f, 0.0f, 1.0f));

	// Масштаб
	model = glm::scale(model, glm::vec3(cube_scale[0], cube_scale[1], cube_scale[2]));

	ubo.model = model;

	// View: камера смотрит на куб
	ubo.view = glm::lookAt(glm::vec3(0.0f, -1.0f, 3.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));

	// Projection: переключение между перспективной и ортографической
	const float aspect = float(graphics::internal::context.swapchain_extent.width) /
	                     float(graphics::internal::context.swapchain_extent.height);

	if (use_orthographic) {
		// Ортографическая проекция — без искажения
		const float ortho_size = 2.0f;  // половина высоты видимой области
		ubo.proj = glm::ortho(-ortho_size * aspect, ortho_size * aspect,
		                      -ortho_size, ortho_size,
		                      -10.0f, 10.0f);
	} else {
		// Перспективная проекция — с искажением
		ubo.proj = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 10.0f);
	}

	// Vulkan использует Y-вниз (OpenGL — Y-вверх), инвертируем Y
	ubo.proj[1][1] *= -1.0f;

	// Копируем данные в uniform buffer
	memcpy(vma_uniform_buffer_info.pMappedData, &ubo, sizeof(ubo));
}

void render(const graphics::internal::FrameData& fd) {
	const VkClearValue clear_values[] = {
		{ .color = {{0.1f, 0.4f, 0.8f, 1.0f}} },   // голубой
		{ .depthStencil = {1.0f, 0} },
	};

	const VkRenderPassBeginInfo render_pass_begin = {
		.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
		.renderPass = fd.render_pass,
		.framebuffer = fd.framebuffer,
		.renderArea = { .extent = fd.extent },
		.clearValueCount = 2,
		.pClearValues = clear_values,
	};

	vkCmdBeginRenderPass(fd.command_buffer, &render_pass_begin, VK_SUBPASS_CONTENTS_INLINE);

	vkCmdBindPipeline(fd.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_pipeline);

		vkCmdBindDescriptorSets(fd.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_pipeline_layout, 0, 1, &vk_descriptor_set, 0, nullptr);

	const VkDeviceSize offsets[] = { 0 };
	vkCmdBindVertexBuffers(fd.command_buffer, 0, 1, &vk_vertex_buffer, offsets);
	vkCmdBindIndexBuffer(fd.command_buffer, vk_index_buffer, 0, VK_INDEX_TYPE_UINT32);
	vkCmdDrawIndexed(fd.command_buffer, index_count, 1, 0, 0, 0);

	vkCmdEndRenderPass(fd.command_buffer);
}

} // namespace application

