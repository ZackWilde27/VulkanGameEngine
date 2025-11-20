#include "Shader.h"
#include "DescriptorSet.h"
#include "engine.h"
#include "VulkanBackend.h"


Shader::Shader(const CHAR_T* zlsl, const CHAR_T* vertfilename, const CHAR_T* pixlfilename, VkRenderPass renderPass, int shaderType, VkExtent2D screenSize, VkCullModeFlags cullMode, VkPolygonMode polygonMode, VkSampleCountFlagBits sampleCount, BlendMode blendMode, bool depthTest, bool depthWrite, VkPushConstantRange* pushConstantRanges, uint32_t numPushConstantRanges, uint32_t stencilWriteMask, VkCompareOp stencilTestOp, uint32_t stencilTestValue, float depthBias, bool masked, VulkanBackend* backend)
{
	device = backend->logicalDevice;
	this->setLayouts = GetDescriptorSetLayoutsFromZLSL(zlsl, &this->numAttachments);

	this->renderPass = renderPass;
	this->shaderType = shaderType;
	this->cullMode = cullMode;
	this->depthBias = depthBias;
	this->depthTest = depthTest;
	this->depthWrite = depthWrite;
	this->blendMode = blendMode;
	this->stencilWriteMask = stencilWriteMask;
	this->stencilCompareOp = stencilTestOp;
	this->stencilTestValue = stencilTestValue;
	this->polygonMode = polygonMode;
	this->sampleCount = sampleCount;
	this->masked = masked;

	this->zlslFile = new zstring(zlsl);
	this->vertexShader = new zstring(vertfilename);
	this->pixelShader = new zstring(pixlfilename);

	this->mtime = FileDate(zlsl);

	CreatePipeline(vertfilename, pixlfilename, screenSize);
}

#ifdef WIDE_STRINGS
Shader::Shader(const char* zlsl, const char* vertfilename, const char* pixlfilename, VkRenderPass renderPass, int shaderType, VkExtent2D screenSize, VkCullModeFlags cullMode, VkPolygonMode polygonMode, VkSampleCountFlagBits sampleCount, BlendMode blendMode, bool depthTest, bool depthWrite, VkPushConstantRange* pushConstantRanges, uint32_t numPushConstantRanges, uint32_t stencilWriteMask, VkCompareOp stencilTestOp, uint32_t stencilTestValue, float depthBias, bool masked, VulkanBackend* backend)
{
	device = backend->logicalDevice;

	this->zlslFile = new zstring(STRING(STRINGFMT), zlsl);
	this->vertexShader = new zstring(STRING(STRINGFMT), vertfilename);
	this->pixelShader = new zstring(STRING(STRINGFMT), pixlfilename);

	this->setLayouts = GetDescriptorSetLayoutsFromZLSL(*zlslFile, &this->numAttachments);

	this->renderPass = renderPass;
	this->shaderType = shaderType;
	this->cullMode = cullMode;
	this->depthBias = depthBias;
	this->depthTest = depthTest;
	this->depthWrite = depthWrite;
	this->blendMode = blendMode;
	this->stencilWriteMask = stencilWriteMask;
	this->stencilCompareOp = stencilTestOp;
	this->stencilTestValue = stencilTestValue;
	this->polygonMode = polygonMode;
	this->sampleCount = sampleCount;
	this->masked = masked;

	this->mtime = FileDate(zlsl);

	CreatePipeline(*vertexShader, *pixelShader, screenSize);
}
#endif

Shader::~Shader()
{
	DestroyPipeline();

	delete zlslFile;
	delete vertexShader;
	delete pixelShader;
}

void Shader::DestroyPipeline()
{
	vkDestroyPipeline(device, pipeline, VK_NULL_HANDLE);
	vkDestroyPipelineLayout(device, pipelineLayout, VK_NULL_HANDLE);
}

void Shader::CreatePipeline(const CHAR_T* vertfilename, const CHAR_T* pixlfilename, VkExtent2D screenSize)
{
	std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
	VkShaderModule vertShaderModule = NULL;
	VkShaderModule pixlShaderModule = NULL;

	if (vertfilename)
	{
		auto vertShaderCode = readFile(vertfilename);
		vertShaderModule = CreateShaderModule(vertShaderCode);
		shaderStages.push_back({});
		shaderStages.back().sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		shaderStages.back().stage = VK_SHADER_STAGE_VERTEX_BIT;
		shaderStages.back().module = vertShaderModule;
		shaderStages.back().pName = "main";
	}

	if (pixlfilename)
	{
		auto pixlShaderCode = readFile(pixlfilename);
		pixlShaderModule = CreateShaderModule(pixlShaderCode);
		shaderStages.push_back({});
		shaderStages.back().sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		shaderStages.back().stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		shaderStages.back().module = pixlShaderModule;
		shaderStages.back().pName = "main";
	}

	VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
	vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

	VkVertexInputBindingDescription bindingDescription;
	std::array<VkVertexInputAttributeDescription, 4> attributeDescriptions;

	if (shaderType != SF_POSTPROCESS && shaderType < SF_SUNSHADOWPASS)
	{
		bindingDescription = Vertex::getBindingDescription();
		attributeDescriptions = Vertex::getAttributeDescriptions();

		vertexInputInfo.vertexBindingDescriptionCount = 1;
		vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
		vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
		vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();
	}
	else
	{
		vertexInputInfo.vertexBindingDescriptionCount = 0;
		vertexInputInfo.vertexAttributeDescriptionCount = 0;
		vertexInputInfo.pVertexBindingDescriptions = VK_NULL_HANDLE;
		vertexInputInfo.pVertexAttributeDescriptions = VK_NULL_HANDLE;
	}

	// The VkPipelineInputAssemblyStateCreateInfo struct describes two things: what kind of geometry will be drawn from the vertices and if primitive restart should be enabled.

	// The former is specified in the topology member and can have values like:
	// VK_PRIMITIVE_TOPOLOGY_POINT_LIST: points from vertices
	// VK_PRIMITIVE_TOPOLOGY_LINE_LIST : line from every 2 vertices without reuse
	// VK_PRIMITIVE_TOPOLOGY_LINE_STRIP : the end vertex of every line is used as start vertex for the next line
	// VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST : triangle from every 3 vertices without reuse
	// VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP : the second and third vertex of every triangle are used as first two vertices of the next triangle

	// Normally, the vertices are loaded from the vertex buffer by index in sequential order, but with an element buffer you can specify the indices to use yourself.
	// This allows you to perform optimizations like reusing vertices.
	// If you set the primitiveRestartEnable member to VK_TRUE, then it's possible to break up lines and triangles in the _STRIP topology modes by using a special index of 0xFFFF or 0xFFFFFFFF.
	VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
	inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssembly.primitiveRestartEnable = VK_FALSE;

	// A viewport basically describes the region of the framebuffer that the output will be rendered to.
	// This will almost always be (0, 0) to (width, height)
	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (float)screenSize.width;
	viewport.height = (float)screenSize.height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	// While viewports define the transformation from the image to the framebuffer, scissor rectangles define in which regions pixels will actually be stored.
	// Any pixels outside the scissor rectangles will be discarded by the rasterizer.
	// They function like a filter rather than a transformation.
	// So if we wanted to draw to the entire framebuffer, we would specify a scissor rectangle that covers it entirely:
	VkRect2D scissor{};
	scissor.offset = { 0, 0 };
	scissor.extent = screenSize;

	// When opting for dynamic viewport(s) and scissor rectangle(s) you need to enable the respective dynamic states for the pipeline:
	std::vector<VkDynamicState> dynamicStates = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR
	};

	VkPipelineDynamicStateCreateInfo dynamicState{};
	dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
	dynamicState.pDynamicStates = dynamicStates.data();

	// And then you only need to specify their count at pipeline creation time:
	VkPipelineViewportStateCreateInfo viewportState{};
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1;
	viewportState.scissorCount = 1;

	// The rasterizer takes the geometry that is shaped by the vertices from the vertex shader and turns it into fragments to be colored by the fragment shader.
	// It also performs depth testing, face culling and the scissor test, and it can be configured to output fragments that fill entire polygons or just the edges (wireframe rendering).
	// All this is configured using the VkPipelineRasterizationStateCreateInfo structure.
	VkPipelineRasterizationStateCreateInfo rasterizer{};
	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;

	// If depthClampEnable is set to VK_TRUE, then fragments that are beyond the near and far planes are clamped to them as opposed to discarding them.
	// This is useful in some special cases like shadow maps. Using this requires enabling a GPU feature.
	rasterizer.depthClampEnable = VK_FALSE;

	// If rasterizerDiscardEnable is set to VK_TRUE, then geometry never passes through the rasterizer stage.This basically disables any output to the framebuffer.
	rasterizer.rasterizerDiscardEnable = VK_FALSE;

	// The polygonMode determines how fragments are generated for geometry. The following modes are available:
	// VK_POLYGON_MODE_FILL: fill the area of the polygon with fragments
	// VK_POLYGON_MODE_LINE : polygon edges are drawn as lines
	// VK_POLYGON_MODE_POINT : polygon vertices are drawn as points

	// Using any mode other than fill requires enabling a GPU feature.
	rasterizer.polygonMode = polygonMode;

	// The lineWidth member is straightforward, it describes the thickness of lines in terms of number of fragments.
	// The maximum line width that is supported depends on the hardware and any line thicker than 1.0f requires you to enable the wideLines GPU feature.
	rasterizer.lineWidth = 1.0f;

	// The cullMode variable determines the type of face culling to use.
	// You can disable culling, cull the front faces, cull the back faces or both.
	rasterizer.cullMode = cullMode;
	// The frontFace variable specifies the vertex order for faces to be considered front - facing and can be clockwise or counterclockwise.
	rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

	// The rasterizer can alter the depth values by adding a constant value or biasing them based on a fragment's slope.
	// Putting in my own comment, I use this to bias the depth in the depth-prepass, without it inaccuracies can cause pixels that are supposed to be drawn think they should be culled.
	rasterizer.depthBiasEnable = depthBias != 0;
	rasterizer.depthBiasConstantFactor = depthBias;
	rasterizer.depthBiasClamp = 0.0f;
	rasterizer.depthBiasSlopeFactor = 0.0f;

	// The VkPipelineMultisampleStateCreateInfo struct configures multisampling, which is one of the ways to perform anti-aliasing.
	// It works by combining the fragment shader results of multiple polygons that rasterize to the same pixel.
	// This mainly occurs along edges, which is also where the most noticeable aliasing artifacts occur.
	// Because it doesn't need to run the fragment shader multiple times if only one polygon maps to a pixel, it is significantly less expensive than simply rendering to a higher resolution and then downscaling.
	// Enabling it requires enabling a GPU feature.
	VkPipelineMultisampleStateCreateInfo multisampling{};
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.sampleShadingEnable = sampleCount > VK_SAMPLE_COUNT_1_BIT;
	multisampling.rasterizationSamples = sampleCount;
	multisampling.minSampleShading = 0.5f; // Optional
	multisampling.pSampleMask = VK_NULL_HANDLE; // Optional
	multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
	multisampling.alphaToOneEnable = VK_FALSE; // Optional

	// After a fragment shader has returned a color, it needs to be combined with the color that is already in the framebuffer.
	// This transformation is known as color blending and there are two ways to do it:
	// - Mix the old and new value to produce a final color
	// - Combine the old and new value using a bitwise operation

	// There are two types of structs to configure color blending.The first struct, VkPipelineColorBlendAttachmentState contains the configuration per attached framebuffer and the second struct, VkPipelineColorBlendStateCreateInfo contains the global color blending settings.
	// In our case we only have one framebuffer

	// The most common way to use color blending is to implement alpha blending, where we want the new color to be blended with the old color based on its opacity.
	// The finalColor should then be computed as follows:
	/*
		finalColor.rgb = newAlpha * newColor + (1 - newAlpha) * oldColor;
		finalColor.a = newAlpha.a;
	*/
	// You can find all of the possible operations in the VkBlendFactor and VkBlendOp enumerations in the specification.
	std::vector<VkPipelineColorBlendAttachmentState> blendAttachments(numAttachments);

	if (numAttachments)
	{
		blendAttachments[0].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

		if (blendMode != BM_OPAQUE)
		{
			blendAttachments[0].blendEnable = VK_TRUE;
			if (blendMode == BM_TRANSPARENT)
			{
				blendAttachments[0].srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
				blendAttachments[0].dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;

				blendAttachments[0].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
				blendAttachments[0].dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
			}
			else
			{
				blendAttachments[0].srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
				blendAttachments[0].dstColorBlendFactor = VK_BLEND_FACTOR_ONE;

				blendAttachments[0].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
				blendAttachments[0].dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
			}

			if (blendMode == BM_MAX)
			{
				blendAttachments[0].colorBlendOp = VK_BLEND_OP_MAX;
				blendAttachments[0].alphaBlendOp = VK_BLEND_OP_MAX;
			}
			else
			{
				blendAttachments[0].colorBlendOp = VK_BLEND_OP_ADD;
				blendAttachments[0].alphaBlendOp = VK_BLEND_OP_ADD;
			}
		}
		else
			blendAttachments[0].blendEnable = VK_FALSE;
	}

	for (uint32_t i = 1; i < numAttachments; i++)
	{
		blendAttachments[i].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		blendAttachments[i].blendEnable = VK_FALSE;
	}

	// The second structure references the array of structures for all of the framebuffers and allows you to set blend constants that you can use as blend factors in the aforementioned calculations.
	VkPipelineColorBlendStateCreateInfo colorBlending{};
	colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlending.logicOpEnable = VK_FALSE;
	colorBlending.logicOp = VK_LOGIC_OP_COPY; // Optional

	colorBlending.attachmentCount = numAttachments;
	colorBlending.pAttachments = blendAttachments.data();
	colorBlending.blendConstants[0] = 0.0f; // Optional
	colorBlending.blendConstants[1] = 0.0f; // Optional
	colorBlending.blendConstants[2] = 0.0f; // Optional
	colorBlending.blendConstants[3] = 0.0f; // Optional

	// You can use uniform values in shaders, which are globals similar to dynamic state variables that can be changed at drawing time to alter the behavior of your shaders without having to recreate them.
	// They are commonly used to pass the transformation matrix to the vertex shader, or to create texture samplers in the fragment shader.

	// These uniform values need to be specified during pipeline creation by creating a VkPipelineLayout object.

	VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	// Tell the pipeline to reference the layout containing shader variables
	pipelineLayoutInfo.setLayoutCount = setLayouts.size();
	pipelineLayoutInfo.pSetLayouts = setLayouts.data();

	pipelineLayoutInfo.pushConstantRangeCount = pushConstantRange.size ? 1 : 0;
	pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

	if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, VK_NULL_HANDLE, &pipelineLayout) != VK_SUCCESS)
		throw std::runtime_error("failed to create pipeline layout!");

	VkPipelineDepthStencilStateCreateInfo depthStencil{};
	depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	// The depthTestEnable field specifies if the depth of new fragments should be compared to the depth buffer to see if they should be discarded.
	depthStencil.depthTestEnable = depthTest;
	// The depthWriteEnable field specifies if the new depth of fragments that pass the depth test should actually be written to the depth buffer.
	depthStencil.depthWriteEnable = depthWrite;

	// The depthCompareOp field specifies the comparison that is performed to keep or discard fragments.
	// We're sticking to the convention of lower depth = closer, so the depth of new fragments should be less
	depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;

	// The depthBoundsTestEnable, minDepthBounds and maxDepthBounds fields are used for the optional depth bound test.
	// Basically, this allows you to only keep fragments that fall within the specified depth range.
	depthStencil.depthBoundsTestEnable = VK_FALSE;
	depthStencil.minDepthBounds = 0.0f; // Optional
	depthStencil.maxDepthBounds = 1.0f; // Optional

	depthStencil.front = {};
	depthStencil.back = {};

	depthStencil.front.failOp = depthStencil.front.depthFailOp = VK_STENCIL_OP_KEEP;

	if (stencilTestValue || stencilWriteMask)
	{
		depthStencil.stencilTestEnable = VK_TRUE;
		depthStencil.front.writeMask = depthStencil.front.compareMask = (uint32_t)-1;

		if (stencilWriteMask)
		{
			depthStencil.front.passOp = VK_STENCIL_OP_REPLACE;
			depthStencil.front.compareOp = VK_COMPARE_OP_ALWAYS;
			depthStencil.front.reference = stencilWriteMask;
		}
		else
		{
			depthStencil.front.passOp = VK_STENCIL_OP_KEEP;
			depthStencil.front.compareOp = stencilCompareOp;
			depthStencil.front.reference = stencilTestValue;
		}
	}
	else
	{
		depthStencil.stencilTestEnable = VK_FALSE;
		depthStencil.front.compareMask = depthStencil.front.reference = depthStencil.front.writeMask = 0;
		depthStencil.front.passOp = VK_STENCIL_OP_KEEP;
		depthStencil.front.compareOp = VK_COMPARE_OP_ALWAYS;
	}

	VkGraphicsPipelineCreateInfo pipelineInfo{};
	pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	// We start by referencing the array of VkPipelineShaderStageCreateInfo structs.
	pipelineInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
	pipelineInfo.pStages = shaderStages.data();
	// Then we reference all of the structures describing the fixed-function stage.
	pipelineInfo.pVertexInputState = &vertexInputInfo;
	pipelineInfo.pInputAssemblyState = &inputAssembly;
	pipelineInfo.pViewportState = &viewportState;
	pipelineInfo.pRasterizationState = &rasterizer;
	pipelineInfo.pMultisampleState = &multisampling;
	pipelineInfo.pDepthStencilState = &depthStencil;
	pipelineInfo.pColorBlendState = (shaderType == SF_SHADOW) ? VK_NULL_HANDLE : &colorBlending;
	pipelineInfo.pDynamicState = &dynamicState;
	// After that comes the pipeline layout, which is a Vulkan handle rather than a struct pointer.
	pipelineInfo.layout = pipelineLayout;

	// And finally we have the reference to the render pass and the index of the sub pass where this graphics pipeline will be used.
	// It is also possible to use other render passes with this pipeline instead of this specific instance, but they have to be compatible with renderPass.
	// The requirements for compatibility are described here, but we won't be using that feature in this tutorial.
	pipelineInfo.renderPass = renderPass;
	pipelineInfo.subpass = 0;

	// There are actually two more parameters: basePipelineHandle and basePipelineIndex. Vulkan allows you to create a new graphics pipeline by deriving from an existing pipeline.
	// The idea of pipeline derivatives is that it is less expensive to set up pipelines when they have much functionality in common with an existing pipeline and switching between pipelines from the same parent can also be done quicker.
	// You can either specify the handle of an existing pipeline with basePipelineHandle or reference another pipeline that is about to be created by index with basePipelineIndex.
	// These values are only used if the VK_PIPELINE_CREATE_DERIVATIVE_BIT flag is also specified in the flags field of VkGraphicsPipelineCreateInfo.
	pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Optional
	pipelineInfo.basePipelineIndex = -1; // Optional

	// The vkCreateGraphicsPipelines function actually has more parameters than the usual object creation functions in Vulkan.
	// It is designed to take multiple VkGraphicsPipelineCreateInfo objects and create multiple VkPipeline objects in a single call.
	// The second parameter, for which we've passed the VK_NULL_HANDLE argument, references an optional VkPipelineCache object.
	// A pipeline cache can be used to store and reuse data relevant to pipeline creation across multiple calls to vkCreateGraphicsPipelines and even across program executions if the cache is stored to a file. This makes it possible to significantly speed up pipeline creation at a later time. We'll get into this in the pipeline cache chapter.
	if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, VK_NULL_HANDLE, &pipeline) != VK_SUCCESS)
		throw std::runtime_error("failed to create graphics pipeline!");

	if (vertShaderModule)
		vkDestroyShaderModule(device, vertShaderModule, VK_NULL_HANDLE);

	if (pixlShaderModule)
		vkDestroyShaderModule(device, pixlShaderModule, VK_NULL_HANDLE);
}

VkShaderModule Shader::CreateShaderModule(const std::vector<char>& code)
{
	VkShaderModuleCreateInfo createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.codeSize = code.size();
	createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

	VkShaderModule shaderModule;
	if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS)
		throw std::runtime_error("failed to create shader module!");

	return shaderModule;
}

void Shader::Recompile(const CHAR_T* vertfilename, const CHAR_T* pixlfilename, VkExtent2D screenSize)
{
	DestroyPipeline();
	CreatePipeline(vertfilename, pixlfilename, screenSize);
}

static char* ReadLayout(char* string, uint32_t* outSet, uint32_t* outBinding)
{
	char* endPtr;

	*outSet = 0;
	*outBinding = 0;

	// Did I mention how much I hate string manipulation in C?

	while (*string++ != '(');

	while (*string != ')')
	{
		if (*string == 's')
		{
			while (*string++ != '=');

			while (isspace(*string))
				string++;

			*outSet = strtol(string, &endPtr, 10);
			string = endPtr;

			while (*string == ',' || isspace(*string))
				string++;
		}
		else if (*string == 'b')
		{
			while (*string++ != '=');

			while (isspace(*string))
				string++;

			*outBinding = strtol(string, &endPtr, 10);
			string = endPtr;

			while (*string == ',' || isspace(*string))
				string++;
		}
		else
			string++;
	}

	return string;
}

// if outVShader is NULL, then it's the same as the zlsl. It will need to be freed afterwards if valid.
template<typename T>
static std::vector<std::array<uint32_t, 4>> GetInfoFromZLSL(const T* zlsl, uint32_t* outAttachments, bool vertexShaderOnly=false)
{
	std::vector<std::array<uint32_t, 4>> setLayoutNums = { { 0, 0, 0, 0 } };

	constexpr uint32_t SAMPLERS = 0;
	constexpr uint32_t VBUFFERS = 1;
	constexpr uint32_t PBUFFERS = 2;
	constexpr uint32_t STORAGE = 3;

	*outAttachments = 0;

	auto buffer = readFile(zlsl);

	bool vertexShader = true;
	char* ptr = buffer.data();
	size_t i = 0;
	int depth = 0;
	size_t shaderLen = 0;
	size_t prevIndex = 0;
	uint32_t set, binding;
	char* tempPtr;

	while (i < buffer.size())
	{
		if (buffer[i] == '[' || buffer[i] == '{' || buffer[i] == '(') depth++;
		if (buffer[i] == ']' || buffer[i] == '}' || buffer[i] == ')') depth--;

		if (!i || ((buffer[i] == ';' || buffer[i] == '\n') && !depth))
		{
			if (buffer[i] == ';') i++;
			while (isspace(buffer[i])) i++;

			switch (buffer[i])
			{
			case 's':
				if (StringCompare(&buffer[i], "sampler2D") || StringCompare(&buffer[i], "samplerCUBE"))
					setLayoutNums[0][SAMPLERS]++;
				break;
			case 'u':
				if (StringCompare(&buffer[i], "uniform"))
				{
					if (vertexShader)
						setLayoutNums[0][VBUFFERS]++;
					else
						setLayoutNums[0][PBUFFERS]++;
				}
				break;
			case 'V':
				if (StringCompare(&buffer[i], "VertexShader"))
				{
					vertexShader = false;
					if (vertexShaderOnly) i = buffer.size();
				}

				break;

			case 'l':
				if (StringCompare(&buffer[i], "layout("))
				{
					tempPtr = ReadLayout(&buffer[i], &set, &binding);
					i += (tempPtr - &buffer[i]);

					while (setLayoutNums.size() <= set)
						setLayoutNums.push_back({ 0, 0, 0, 0 });

					i++;

					while (isspace(buffer[i])) i++;

					if (StringCompare(&buffer[i], "out"))
						(*outAttachments)++;
					else if (StringCompare(&buffer[i], "readonly buffer"))
						setLayoutNums[set][STORAGE]++;
					else if (StringCompare(&buffer[i], "uniform"))
					{
						i += 8;

						while (isspace(buffer[i])) i++;

						if (StringCompare(&buffer[i], "sampler2D") || StringCompare(&buffer[i], "samplerCUBE"))
							setLayoutNums[set][SAMPLERS]++;
						else
						{
							if (vertexShader)
								setLayoutNums[set][VBUFFERS]++;
							else
								setLayoutNums[set][PBUFFERS]++;
						}
					}
				}

				break;

			case '#':
				if (StringCompare(&buffer[i], "#VertexShader"))
				{
					i += 14;
					while (isspace(buffer[i])) i++;

					i++;
					prevIndex = i;
					while (!isspace(buffer[i]))
					{
						shaderLen++;
						i++;
					}

					shaderLen--;

					auto tempBuffer = (char*)malloc(shaderLen + 1);
					check(tempBuffer, "Failed to allocate in GetInfoFromZLSL()!");
					StringCopy(tempBuffer, &buffer[prevIndex], shaderLen);
					tempBuffer[shaderLen] = NULL;

					char vshaderBuffer[256];
					StringCopySafe(vshaderBuffer, 256, "shaders/");
					StringConcatSafe(vshaderBuffer, 256, tempBuffer);

					setLayoutNums = GetInfoFromZLSL(vshaderBuffer, outAttachments, true);

					free(tempBuffer);
					vertexShader = false;
				}
				break;

			default:
				break;
			}
		}
		i++;
	}

	return setLayoutNums;
}

std::vector<VkDescriptorSetLayout> Shader::GetDescriptorSetLayoutsFromZLSL(const CHAR_T* filename, uint32_t* outAttachments)
{
	std::vector<VkDescriptorSetLayout> layouts;

	auto setLayoutNums = GetInfoFromZLSL(filename, outAttachments);
	for (const auto& nums : setLayoutNums)
		layouts.push_back(*DescriptorSet::GetDescriptorSetLayout(nums[1], nums[2], nums[0], nums[3]));

	return layouts;
}