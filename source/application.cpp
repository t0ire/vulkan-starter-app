#include "application.hpp"

#include <vulkan/vulkan.h>
#include <imgui.h>
#include <fstream>
#include <vector>
#include <iostream>
#include <cstring>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <string>

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
	float normal[3];
};

Vertex makeVertex(float x, float y, float z, float nx, float ny, float nz) {
	Vertex v;
	v.position[0] = x;
	v.position[1] = y;
	v.position[2] = z;

	v.color[0] = (x + 0.3f) / 0.6f;
	v.color[1] = (y + 0.3f) / 0.6f;
	v.color[2] = (z + 0.3f) / 0.6f;

	v.normal[0] = nx;
	v.normal[1] = ny;
	v.normal[2] = nz;
	return v;
}

Vertex cube_vertices[] = {
	//задняя грань (Z = -0.3), нормаль (0, 0, -1)
	makeVertex(-0.3f, -0.3f, -0.3f,  0.0f,  0.0f, -1.0f),
	makeVertex( 0.3f, -0.3f, -0.3f,  0.0f,  0.0f, -1.0f),
	makeVertex( 0.3f,  0.3f, -0.3f,  0.0f,  0.0f, -1.0f),
	makeVertex(-0.3f,  0.3f, -0.3f,  0.0f,  0.0f, -1.0f),
	//передняя грань (Z = 0.3), нормаль (0, 0, 1)
	makeVertex(-0.3f, -0.3f,  0.3f,  0.0f,  0.0f,  1.0f),
	makeVertex( 0.3f, -0.3f,  0.3f,  0.0f,  0.0f,  1.0f),
	makeVertex( 0.3f,  0.3f,  0.3f,  0.0f,  0.0f,  1.0f),
	makeVertex(-0.3f,  0.3f,  0.3f,  0.0f,  0.0f,  1.0f),
	//левая грань (X = -0.3), нормаль (-1, 0, 0)
	makeVertex(-0.3f, -0.3f, -0.3f, -1.0f,  0.0f,  0.0f),
	makeVertex(-0.3f,  0.3f, -0.3f, -1.0f,  0.0f,  0.0f),
	makeVertex(-0.3f,  0.3f,  0.3f, -1.0f,  0.0f,  0.0f),
	makeVertex(-0.3f, -0.3f,  0.3f, -1.0f,  0.0f,  0.0f),
	//правая грань (X = 0.3), нормаль (1, 0, 0)
	makeVertex( 0.3f, -0.3f, -0.3f,  1.0f,  0.0f,  0.0f),
	makeVertex( 0.3f, -0.3f,  0.3f,  1.0f,  0.0f,  0.0f),
	makeVertex( 0.3f,  0.3f,  0.3f,  1.0f,  0.0f,  0.0f),
	makeVertex( 0.3f,  0.3f, -0.3f,  1.0f,  0.0f,  0.0f),
	//нижняя грань (Y = -0.3), нормаль (0, -1, 0)
	makeVertex(-0.3f, -0.3f, -0.3f,  0.0f, -1.0f,  0.0f),
	makeVertex( 0.3f, -0.3f, -0.3f,  0.0f, -1.0f,  0.0f),
	makeVertex( 0.3f, -0.3f,  0.3f,  0.0f, -1.0f,  0.0f),
	makeVertex(-0.3f, -0.3f,  0.3f,  0.0f, -1.0f,  0.0f),
	//верхняя грань (Y = 0.3), нормаль (0, 1, 0)
	makeVertex(-0.3f,  0.3f, -0.3f,  0.0f,  1.0f,  0.0f),
	makeVertex(-0.3f,  0.3f,  0.3f,  0.0f,  1.0f,  0.0f),
	makeVertex( 0.3f,  0.3f,  0.3f,  0.0f,  1.0f,  0.0f),
	makeVertex( 0.3f,  0.3f, -0.3f,  0.0f,  1.0f,  0.0f),
};

const uint32_t cube_indices[] = {
	// Задняя грань (смотрим сзади, Z = -0.3)
	0, 3, 2,  0, 2, 1,
	
	// Передняя грань (смотрим спереди, Z = 0.3)
	4, 5, 6,  4, 6, 7,
	
	// Левая грань (смотрим слева, X = -0.3)
	8, 11, 10,  8, 10, 9,
	
	// Правая грань (смотрим справа, X = 0.3)
	12, 13, 14,  12, 14, 15,
	
	// Нижняя грань (смотрим снизу, Y = -0.3)
	16, 17, 18,  16, 18, 19,
	
	// Верхняя грань (смотрим сверху, Y = 0.3)
	20, 23, 22,  20, 22, 21
};

struct SceneUBO {
	glm::mat4 view;
	glm::mat4 proj;
	glm::vec4 cameraPos;     // xyz = позиция камеры, w = padding
	glm::vec4 lightDir;      // xyz = направление света, w = padding
	glm::vec4 lightColor;    // xyz = цвет, w = интенсивность
	glm::vec4 ambientColor;  // xyz = фоновый свет, w = padding
};

struct ModelUBO {
	glm::mat4 model;
	glm::vec4 albedo;        // xyz = базовый цвет, w = shininess
	glm::vec4 specularColor; // xyz = цвет блика, w = padding
};

struct PointLight {
	glm::vec4 position;  // xyz = позиция, w = padding
	glm::vec4 color;     // xyz = цвет, w = интенсивность
};

//массив точечных источников 
const int MAX_POINT_LIGHTS = 10;
std::vector<PointLight> point_lights = {
	{ glm::vec4(2.0f, 2.0f, 2.0f, 1.0f), glm::vec4(1.0f, 0.0f, 0.0f, 5.0f) }, 
	{ glm::vec4(-2.0f, -2.0f, 2.0f, 1.0f), glm::vec4(0.0f, 1.0f, 0.0f, 5.0f) } 
};

struct SpotLight {
	glm::vec4 position;    // xyz = позиция, w = padding
	glm::vec4 direction;   // xyz = направление, w = padding
	glm::vec4 color;       // xyz = цвет, w = интенсивность
	glm::vec4 params;      // x = cos(innerAngle), y = cos(outerAngle), z/w = padding
};

const int MAX_SPOT_LIGHTS = 10;
std::vector<SpotLight> spot_lights = {
	{
		glm::vec4(0.0f, 5.0f, 0.0f, 1.0f),    //сверху
		glm::vec4(0.0f, -1.0f, 0.0f, 0.0f),   //вниз
		glm::vec4(1.0f, 1.0f, 1.0f, 10.0f),   //сolor + intensity
		glm::vec4(glm::cos(glm::radians(15.0f)), glm::cos(glm::radians(25.0f)), 0.0f, 0.0f)
	}
};

//storage Buffer для прожекторов
VkBuffer vk_spot_lights_buffer;
VmaAllocation vma_spot_lights_buffer_allocation;
VmaAllocationInfo vma_spot_lights_buffer_info;

//буферы для сцены и модели
VkBuffer vk_scene_uniform_buffer;
VmaAllocation vma_scene_uniform_buffer_allocation;
VmaAllocationInfo vma_scene_uniform_buffer_info;

VkBuffer vk_model_uniform_buffer;
VmaAllocation vma_model_uniform_buffer_allocation;
VmaAllocationInfo vma_model_uniform_buffer_info;

//дескрипторы (Layout, Pool, Sets)
VkDescriptorSetLayout vk_scene_descriptor_set_layout;
VkDescriptorSetLayout vk_model_descriptor_set_layout;
VkDescriptorPool vk_descriptor_pool;
VkDescriptorSet vk_scene_descriptor_set;
VkDescriptorSet vk_model_descriptor_set;

//Storage Buffer для точечных источников
VkBuffer vk_point_lights_buffer;
VmaAllocation vma_point_lights_buffer_allocation;
VmaAllocationInfo vma_point_lights_buffer_info;

bool use_orthographic = false;

float cube_position[3] = { 0.0f, 0.0f, 0.0f }; //проекция
float cube_rotation[3] = { 0.0f, 0.0f, 0.0f }; //поворот в градусах
float cube_scale[3]    = { 1.0f, 1.0f, 1.0f }; //масштаб

bool is_animating = false; //анимация
float animation_speed = 1.0f;
float animation_radius = 1.5f; 
float animation_time = 0.0f; //общее время
double last_time = 0.0; //ласт время 
float cube_color[4] = { 1.0f, 1.0f, 1.0f, 1.0f };  // белый

float camera_pos[3] = { -4.0f, 3.0f, 6.0f }; 
float camera_fov = 30.0f;

float light_dir[3] = { 1.0f, 1.0f, 1.0f };
float light_color[3] = { 1.0f, 1.0f, 1.0f };
float light_intensity = 1.0f;
float ambient_color[3] = { 0.1f, 0.1f, 0.1f };

float material_specular[3] = { 1.0f, 1.0f, 1.0f };
float material_shininess = 32.0f;
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
		.binding = 0,
		.stride = sizeof(Vertex), 
		.inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
	};

		const VkVertexInputAttributeDescription vertex_attributes[] = {
		{ 
			.location = 0, 
			.binding = 0, 
			.format = VK_FORMAT_R32G32B32_SFLOAT, 
			.offset = offsetof(Vertex, position) 
		},
		{ 
			.location = 1, 
			.binding = 0, 
			.format = VK_FORMAT_R32G32B32_SFLOAT, 
			.offset = offsetof(Vertex, color) 
		},
		{ 
			.location = 2, 
			.binding = 0, 
			.format = VK_FORMAT_R32G32B32_SFLOAT, 
			.offset = offsetof(Vertex, normal) 
		}, 
	};

	const VkPipelineVertexInputStateCreateInfo vertex_input = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
		.vertexBindingDescriptionCount = 1,
		.pVertexBindingDescriptions = &vertex_binding,
		.vertexAttributeDescriptionCount = 3,
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
		.cullMode = VK_CULL_MODE_NONE,    // отбрасывать задние грани
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

	const VkDescriptorSetLayout set_layouts[] = {
		vk_scene_descriptor_set_layout, // Set 0
		vk_model_descriptor_set_layout  // Set 1
	};
	const VkPipelineLayoutCreateInfo pipeline_layout_info = {
		.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		.setLayoutCount = 2, .pSetLayouts = set_layouts,
		.pushConstantRangeCount = 0, .pPushConstantRanges = nullptr,
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

	// копируем данные вершин в память GPU
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

bool createSceneUniformBuffer() {
	const VkDeviceSize buffer_size = sizeof(SceneUBO);
	const VkBufferCreateInfo buffer_info = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, .size = buffer_size,
		.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
	};
	const VmaAllocationCreateInfo allocation_info = {
		.usage = VMA_MEMORY_USAGE_AUTO,
		.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
	};
	if (vmaCreateBuffer(graphics::internal::context.allocator, &buffer_info, &allocation_info,
	                    &vk_scene_uniform_buffer, &vma_scene_uniform_buffer_allocation,
	                    &vma_scene_uniform_buffer_info) != VK_SUCCESS) {
		std::cerr << "Failed to create scene uniform buffer\n";
		return false;
	}
	std::cout << "Scene UBO created successfully\n";
	return true;
}

bool createModelUniformBuffer() {
	const VkDeviceSize buffer_size = sizeof(ModelUBO);
	const VkBufferCreateInfo buffer_info = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO, .size = buffer_size,
		.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
	};
	const VmaAllocationCreateInfo allocation_info = {
		.usage = VMA_MEMORY_USAGE_AUTO,
		.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT,
	};
	if (vmaCreateBuffer(graphics::internal::context.allocator, &buffer_info, &allocation_info,
	                    &vk_model_uniform_buffer, &vma_model_uniform_buffer_allocation,
	                    &vma_model_uniform_buffer_info) != VK_SUCCESS) {
		std::cerr << "Failed to create model uniform buffer\n";
		return false;
	}
	std::cout << "Model UBO created successfully\n";
	return true;
}

bool createPointLightsBuffer() {
	//резервируем память под максимальное количество источников
	const VkDeviceSize buffer_size = sizeof(PointLight) * MAX_POINT_LIGHTS;

	const VkBufferCreateInfo buffer_info = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = buffer_size,
		.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,  
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
	};

	const VmaAllocationCreateInfo allocation_info = {
		.usage = VMA_MEMORY_USAGE_AUTO,
		.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
		         VMA_ALLOCATION_CREATE_MAPPED_BIT,
	};

	if (vmaCreateBuffer(graphics::internal::context.allocator, &buffer_info, &allocation_info,
	                    &vk_point_lights_buffer, &vma_point_lights_buffer_allocation,
	                    &vma_point_lights_buffer_info) != VK_SUCCESS) {
		std::cerr << "Failed to create point lights storage buffer\n";
		return false;
	}

	std::cout << "Point Lights Storage Buffer created successfully\n";
	return true;
}

bool createSpotLightsBuffer() {
	const VkDeviceSize buffer_size = sizeof(SpotLight) * MAX_SPOT_LIGHTS;

	const VkBufferCreateInfo buffer_info = {
		.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
		.size = buffer_size,
		.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
		.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
	};

	const VmaAllocationCreateInfo allocation_info = {
		.usage = VMA_MEMORY_USAGE_AUTO,
		.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
		         VMA_ALLOCATION_CREATE_MAPPED_BIT,
	};

	if (vmaCreateBuffer(graphics::internal::context.allocator, &buffer_info, &allocation_info,
	                    &vk_spot_lights_buffer, &vma_spot_lights_buffer_allocation,
	                    &vma_spot_lights_buffer_info) != VK_SUCCESS) {
		std::cerr << "Failed to create spot lights storage buffer\n";
		return false;
	}

	std::cout << "Spot Lights Storage Buffer created successfully\n";
	return true;
}

bool createSceneDescriptorSetLayout() {
	const VkDescriptorSetLayoutBinding bindings[] = {
		{
			.binding = 0,
			.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			.descriptorCount = 1,
			.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
		},
		{
			.binding = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			.descriptorCount = 1,
			.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
		},
		{
			.binding = 2,                                     
			.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			.descriptorCount = 1,
			.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
		}
	};

	const VkDescriptorSetLayoutCreateInfo layout_info = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		.bindingCount = 3,                                     
		.pBindings = bindings,
	};

	if (vkCreateDescriptorSetLayout(graphics::internal::context.device, &layout_info, nullptr, &vk_scene_descriptor_set_layout) != VK_SUCCESS) {
		return false;
	}
	std::cout << "Scene Layout created successfully\n";
	return true;
}

bool createModelDescriptorSetLayout() {
	const VkDescriptorSetLayoutBinding binding = {
		.binding = 0, .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount = 1,
		.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
	};
	const VkDescriptorSetLayoutCreateInfo layout_info = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO, .bindingCount = 1, .pBindings = &binding,
	};
	if (vkCreateDescriptorSetLayout(graphics::internal::context.device, &layout_info, nullptr, &vk_model_descriptor_set_layout) != VK_SUCCESS) {
		return false;
	}
	std::cout << "Model Layout created successfully\n";
	return true;
}

bool createDescriptorPool() {
	const VkDescriptorPoolSize pool_sizes[] = {
		{ .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount = 2 }, //для Scene и Model
		{ .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .descriptorCount = 2 }  //для Point Lights
	};

	const VkDescriptorPoolCreateInfo pool_info = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
		.poolSizeCount = 2,
		.pPoolSizes = pool_sizes,
		.maxSets = 2,
	};

	if (vkCreateDescriptorPool(graphics::internal::context.device, &pool_info, nullptr, &vk_descriptor_pool) != VK_SUCCESS) {
		return false;
	}
	return true;
}

bool createDescriptorSets() {
	//сцена
	const VkDescriptorSetAllocateInfo scene_alloc = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, .descriptorPool = vk_descriptor_pool,
		.descriptorSetCount = 1, .pSetLayouts = &vk_scene_descriptor_set_layout,
	};
	if (vkAllocateDescriptorSets(graphics::internal::context.device, &scene_alloc, &vk_scene_descriptor_set) != VK_SUCCESS) return false;

	const VkDescriptorBufferInfo scene_buf = { .buffer = vk_scene_uniform_buffer, .offset = 0, .range = sizeof(SceneUBO) };
	const VkDescriptorBufferInfo point_lights_buf = { .buffer = vk_point_lights_buffer, .offset = 0, .range = VK_WHOLE_SIZE };
	const VkDescriptorBufferInfo spot_lights_buf = { .buffer = vk_spot_lights_buffer, .offset = 0, .range = VK_WHOLE_SIZE };

	const VkWriteDescriptorSet scene_writes[] = {
		{
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = vk_scene_descriptor_set,
			.dstBinding = 0, .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount = 1, .pBufferInfo = &scene_buf,
		},
		{
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = vk_scene_descriptor_set,
			.dstBinding = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .descriptorCount = 1, .pBufferInfo = &point_lights_buf,
		},
		{
			.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = vk_scene_descriptor_set,
			.dstBinding = 2, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .descriptorCount = 1, .pBufferInfo = &spot_lights_buf,
		}
	};

	vkUpdateDescriptorSets(graphics::internal::context.device, 3, scene_writes, 0, nullptr);

	//модель
	const VkDescriptorSetAllocateInfo model_alloc = {
		.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO, .descriptorPool = vk_descriptor_pool,
		.descriptorSetCount = 1, .pSetLayouts = &vk_model_descriptor_set_layout,
	};
	if (vkAllocateDescriptorSets(graphics::internal::context.device, &model_alloc, &vk_model_descriptor_set) != VK_SUCCESS) return false;

	const VkDescriptorBufferInfo model_buf = { .buffer = vk_model_uniform_buffer, .offset = 0, .range = sizeof(ModelUBO) };
	const VkWriteDescriptorSet model_write = {
		.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = vk_model_descriptor_set,
		.dstBinding = 0, .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount = 1, .pBufferInfo = &model_buf,
	};
	vkUpdateDescriptorSets(graphics::internal::context.device, 1, &model_write, 0, nullptr);

	std::cout << "Descriptor Sets created successfully\n";
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

	if (!createSceneDescriptorSetLayout()) return false;
	if (!createModelDescriptorSetLayout()) return false;

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

	if (!createSceneUniformBuffer()) return false;
	if (!createModelUniformBuffer()) return false;
	if (!createPointLightsBuffer()) return false;
	if (!createSpotLightsBuffer()) return false;
	if (!createDescriptorPool()) return false;
	if (!createDescriptorSets()) return false;

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
	vkDestroyDescriptorSetLayout(context.device, vk_scene_descriptor_set_layout, nullptr);
	vkDestroyDescriptorSetLayout(context.device, vk_model_descriptor_set_layout, nullptr);
	vmaDestroyBuffer(context.allocator, vk_scene_uniform_buffer, vma_scene_uniform_buffer_allocation);
	vmaDestroyBuffer(context.allocator, vk_model_uniform_buffer, vma_model_uniform_buffer_allocation);
	vmaDestroyBuffer(context.allocator, vk_point_lights_buffer, vma_point_lights_buffer_allocation);
	vmaDestroyBuffer(context.allocator, vk_spot_lights_buffer, vma_spot_lights_buffer_allocation);
}

void update([[maybe_unused]] double time) {
	ImGui::ShowDemoWindow();

	ImGui::Begin("Controls");

	ImGui::Checkbox("Orthographic projection", &use_orthographic);

	ImGui::SetNextItemWidth(300.0f);
	ImGui::SliderFloat3("Position", cube_position, -3.0f, 3.0f);

	ImGui::SetNextItemWidth(300.0f);
	ImGui::SliderFloat3("Rotation", cube_rotation, -180.0f, 180.0f);

	ImGui::SetNextItemWidth(300.0f);
	ImGui::SliderFloat3("Scale", cube_scale, 0.1f, 3.0f);

	ImGui::SetNextItemWidth(300.0f);
	ImGui::ColorEdit3("Color", cube_color);

	ImGui::Separator();
	ImGui::Text("Animation");
	ImGui::Checkbox("Play", &is_animating);
	ImGui::SetNextItemWidth(300.0f);
	ImGui::SliderFloat("Speed", &animation_speed, 0.0f, 5.0f);
	ImGui::SetNextItemWidth(300.0f);
	ImGui::SliderFloat("Radius", &animation_radius, 0.0f, 3.0f);

	ImGui::Separator();
	ImGui::Text("Camera");
	ImGui::SliderFloat3("Cam Pos", camera_pos, -10.0f, 10.0f);
	ImGui::SliderFloat("Cam FOV", &camera_fov, 10.0f, 90.0f);

	ImGui::Separator();
	ImGui::Text("Light");
	ImGui::SliderFloat3("Light Dir", light_dir, -1.0f, 1.0f);
	ImGui::ColorEdit3("Light Color", light_color);
	ImGui::SliderFloat("Intensity", &light_intensity, 0.0f, 5.0f);
	ImGui::ColorEdit3("Ambient", ambient_color);

	ImGui::Separator();
	ImGui::Text("Material");
	ImGui::ColorEdit3("Specular", material_specular);
	ImGui::SliderFloat("Shininess", &material_shininess, 1.0f, 256.0f);

	ImGui::Separator();
	ImGui::Text("Point Lights");
	
	for (size_t i = 0; i < point_lights.size(); ++i) {
		ImGui::PushID(int(i));
		if (ImGui::TreeNode(("Light #" + std::to_string(i)).c_str())) {
			ImGui::SetNextItemWidth(300.0f);
			ImGui::SliderFloat3("Position", &point_lights[i].position.x, -5.0f, 5.0f);
			ImGui::ColorEdit3("Color", &point_lights[i].color.x);
			ImGui::SetNextItemWidth(300.0f);
			ImGui::SliderFloat("Intensity", &point_lights[i].color.w, 0.0f, 10.0f);
			
			if (ImGui::Button("Remove")) {
				point_lights.erase(point_lights.begin() + i);
			}
			ImGui::TreePop();
		}
		ImGui::PopID();
	}
	
	if (point_lights.size() < MAX_POINT_LIGHTS) {
		if (ImGui::Button("Add Point Light")) {
			PointLight new_light = {
				{ 0.0f, 3.0f, 0.0f, 1.0f },  // Позиция
				{ 1.0f, 1.0f, 1.0f, 5.0f }   // Белый свет
			};
			point_lights.push_back(new_light);
		}
	}

	ImGui::End();

	const double current_time = time;
	const double delta_time = current_time - last_time;
	last_time = current_time;

	if (is_animating) {
		animation_time += float(delta_time) * animation_speed;
	}

	SceneUBO scene_ubo{};
	ModelUBO model_ubo{};

	// вращаем куб вокруг оси Y
	glm::mat4 model = glm::mat4(1.0f);

	// автоматическая позиция от анимации 
	float auto_pos_x = 0.0f;
	float auto_pos_z = 0.0f;
	float auto_rot_y = 0.0f;

	if (is_animating) {
		auto_pos_x = glm::cos(animation_time) * animation_radius;
		auto_pos_z = glm::sin(animation_time) * animation_radius;
		auto_rot_y = animation_time * 45.0f;
	}

	// позиция = слайдер + анимация
	model = glm::translate(model, glm::vec3(
		cube_position[0] + auto_pos_x,
		cube_position[1],
		cube_position[2] + auto_pos_z
	));

	// поворот = слайдер + анимация
	model = glm::rotate(model, glm::radians(cube_rotation[0]), glm::vec3(1.0f, 0.0f, 0.0f));
	model = glm::rotate(model, glm::radians(cube_rotation[1] + auto_rot_y), glm::vec3(0.0f, 1.0f, 0.0f));
	model = glm::rotate(model, glm::radians(cube_rotation[2]), glm::vec3(0.0f, 0.0f, 1.0f));

	// масштаб
	model = glm::scale(model, glm::vec3(cube_scale[0], cube_scale[1], cube_scale[2]));

	model_ubo.model = model;
	model_ubo.albedo = glm::vec4(cube_color[0], cube_color[1], cube_color[2], material_shininess);
	model_ubo.specularColor = glm::vec4(material_specular[0], material_specular[1], material_specular[2], 1.0f);


	// View: камера смотрит на куб
	scene_ubo.view = glm::lookAt(glm::vec3(camera_pos[0], camera_pos[1], camera_pos[2]), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));

	// Projection: переключение между перспективной и ортографической
	const float aspect = float(graphics::internal::context.swapchain_extent.width) /
	                     float(graphics::internal::context.swapchain_extent.height);

	if (use_orthographic) {
		// ортографическая проекция
		const float ortho_size = 2.0f;  // половина высоты видимой области
		scene_ubo.proj = glm::ortho(-ortho_size * aspect, ortho_size * aspect,
		                      -ortho_size, ortho_size,
		                      -10.0f, 10.0f);
	} else {
		// перспективная проекция 
	scene_ubo.proj = glm::perspective(glm::radians(camera_fov), aspect, 0.1f, 10.0f);
	}

	scene_ubo.proj[1][1] *= -1.0f;

	scene_ubo.cameraPos = glm::vec4(camera_pos[0], camera_pos[1], camera_pos[2], 1.0f);
	scene_ubo.lightDir = glm::vec4(glm::normalize(glm::vec3(light_dir[0], light_dir[1], light_dir[2])), 0.0f);
	scene_ubo.lightColor = glm::vec4(light_color[0], light_color[1], light_color[2], light_intensity);
	scene_ubo.ambientColor = glm::vec4(ambient_color[0], ambient_color[1], ambient_color[2], 1.0f);

	memcpy(vma_scene_uniform_buffer_info.pMappedData, &scene_ubo, sizeof(scene_ubo));
	memcpy(vma_model_uniform_buffer_info.pMappedData, &model_ubo, sizeof(model_ubo));
	memcpy(vma_point_lights_buffer_info.pMappedData, point_lights.data(), point_lights.size() * sizeof(PointLight));
	memcpy(vma_spot_lights_buffer_info.pMappedData, spot_lights.data(), spot_lights.size() * sizeof(SpotLight));
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

	const VkDescriptorSet sets[] = { vk_scene_descriptor_set, vk_model_descriptor_set };
	vkCmdBindDescriptorSets(fd.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_pipeline_layout, 0, 2, sets, 0, nullptr);

	const VkDeviceSize offsets[] = { 0 };
	vkCmdBindVertexBuffers(fd.command_buffer, 0, 1, &vk_vertex_buffer, offsets);
	vkCmdBindIndexBuffer(fd.command_buffer, vk_index_buffer, 0, VK_INDEX_TYPE_UINT32);
	vkCmdDrawIndexed(fd.command_buffer, index_count, 1, 0, 0, 0);

	vkCmdEndRenderPass(fd.command_buffer);
}

} // namespace application

