#include "RendererPost.h"
#include "Logger.h"

namespace guk {

RendererPost::RendererPost(std::shared_ptr<Device> device, VkFormat colorFormat, uint32_t width,
                           uint32_t height, std::shared_ptr<Image2D> sceneTexture)
    : device_(device), sceneTexture_(sceneTexture),
      postUniformBuffers_(std::make_unique<Buffer>(device_)),
      bloomTexture_(std::make_unique<Image2D>(device_))
{
    createAttachments(width, height);
    createUniform();

    createDescriptorSetLayout();
    allocateDescriptorSets();
    updateDescriptorSets();
    createPipelineLayout();
    createPipeline(colorFormat);
}

RendererPost::~RendererPost()
{
    vkDestroyPipeline(device_->get(), pipeline_, nullptr);
    vkDestroyPipelineLayout(device_->get(), pipelineLayout_, nullptr);
    vkDestroyDescriptorSetLayout(device_->get(), descriptorSetLayout_, nullptr);
}

void RendererPost::updatePost(uint32_t frameIdx, PostUniform postUniform)
{
    postUniformBuffers_[frameIdx]->update(postUniform);
}

void RendererPost::draw(VkCommandBuffer cmd, uint32_t frameIdx,
                        std::shared_ptr<Image2D> swapchainImg)
{
    sceneTexture_->transition(cmd, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                              VK_ACCESS_2_SHADER_READ_BIT,
                              VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    bloomTexture_->transition(cmd, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                              VK_ACCESS_2_SHADER_READ_BIT,
                              VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    VkRenderingAttachmentInfo colorAttachment{};
    colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    colorAttachment.imageView = swapchainImg->view();
    colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.clearValue.color = {0.f, 0.f, 0.f, 1.f};

    VkRenderingInfo renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderingInfo.renderArea = {0, 0, swapchainImg->width(), swapchainImg->height()};
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments = &colorAttachment;

    vkCmdBeginRendering(cmd, &renderingInfo);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);

    VkViewport viewport{};
    viewport.x = 0.f;
    viewport.y = 0.f;
    viewport.width = static_cast<float>(swapchainImg->width());
    viewport.height = static_cast<float>(swapchainImg->height());
    viewport.minDepth = 0.f;
    viewport.maxDepth = 1.f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = {swapchainImg->width(), swapchainImg->height()};
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout_, 0, 1,
                            &descriptorSets_[frameIdx], 0, nullptr);

    vkCmdDraw(cmd, 6, 1, 0, 0);

    vkCmdEndRendering(cmd);
}

void RendererPost::resized(uint32_t width, uint32_t height)
{
    createAttachments(width, height);
    updateDescriptorSets();
}

void RendererPost::createAttachments(uint32_t width, uint32_t height)
{
    bloomTexture_->createImage(VK_FORMAT_R16G16B16A16_SFLOAT, width, height,
                               VK_IMAGE_USAGE_SAMPLED_BIT, VK_SAMPLE_COUNT_1_BIT, BLOOM_LEVEL);
    bloomTexture_->setSampler(device_->samplerLinearClamp());

    for (uint32_t i = 0; i < BLOOM_LEVEL; i++) {
        bloomAttachemnts_[i] = std::make_unique<Image2D>(device_);
        bloomAttachemnts_[i]->createView(bloomTexture_, i, 1);
    }
}

void RendererPost::createUniform()
{
    for (uint32_t i = 0; i < Device::MAX_FRAMES_IN_FLIGHT; i++) {
        postUniformBuffers_[i] = std::make_unique<Buffer>(device_);
        postUniformBuffers_[i]->createUniformBuffers(sizeof(PostUniform));
    }
}

void RendererPost::createDescriptorSetLayout()
{
    std::array<VkDescriptorSetLayoutBinding, 3> descSetLayoutBinding{};
    descSetLayoutBinding[0].binding = 0;
    descSetLayoutBinding[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descSetLayoutBinding[0].descriptorCount = 1;
    descSetLayoutBinding[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    descSetLayoutBinding[1].binding = 1;
    descSetLayoutBinding[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descSetLayoutBinding[1].descriptorCount = 1;
    descSetLayoutBinding[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    descSetLayoutBinding[2].binding = 2;
    descSetLayoutBinding[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descSetLayoutBinding[2].descriptorCount = 1;
    descSetLayoutBinding[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo descSetLayoutCI{};
    descSetLayoutCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descSetLayoutCI.bindingCount = static_cast<uint32_t>(descSetLayoutBinding.size());
    descSetLayoutCI.pBindings = descSetLayoutBinding.data();

    VK_CHECK(vkCreateDescriptorSetLayout(device_->get(), &descSetLayoutCI, nullptr,
                                         &descriptorSetLayout_));
}

void RendererPost::allocateDescriptorSets()
{
    std::vector<VkDescriptorSetLayout> layouts(Device::MAX_FRAMES_IN_FLIGHT, descriptorSetLayout_);
    VkDescriptorSetAllocateInfo descSetAI{};
    descSetAI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descSetAI.descriptorPool = device_->descriptorPool();
    descSetAI.descriptorSetCount = Device::MAX_FRAMES_IN_FLIGHT;
    descSetAI.pSetLayouts = layouts.data();

    VK_CHECK(vkAllocateDescriptorSets(device_->get(), &descSetAI, descriptorSets_.data()));
}

void RendererPost::updateDescriptorSets()
{
    for (size_t i = 0; i < Device::MAX_FRAMES_IN_FLIGHT; i++) {
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = postUniformBuffers_[i]->get();
        bufferInfo.range = sizeof(PostUniform);

        VkDescriptorImageInfo sceneImageInfo{};
        sceneImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        sceneImageInfo.imageView = sceneTexture_->view();
        sceneImageInfo.sampler = sceneTexture_->sampler();

        VkDescriptorImageInfo bloomImageInfo{};
        bloomImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        bloomImageInfo.imageView = bloomTexture_->view();
        bloomImageInfo.sampler = bloomTexture_->sampler();

        std::array<VkWriteDescriptorSet, 3> write{};
        write[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write[0].dstSet = descriptorSets_[i];
        write[0].dstBinding = 0;
        write[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        write[0].descriptorCount = 1;
        write[0].pBufferInfo = &bufferInfo;

        write[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write[1].dstSet = descriptorSets_[i];
        write[1].dstBinding = 1;
        write[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        write[1].descriptorCount = 1;
        write[1].pImageInfo = &sceneImageInfo;

        write[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write[2].dstSet = descriptorSets_[i];
        write[2].dstBinding = 2;
        write[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        write[2].descriptorCount = 1;
        write[2].pImageInfo = &bloomImageInfo;

        vkUpdateDescriptorSets(device_->get(), static_cast<uint32_t>(write.size()), write.data(), 0,
                               nullptr);
    }
}

void RendererPost::createPipelineLayout()
{
    VkPipelineLayoutCreateInfo pipelineLayoutCI{};
    pipelineLayoutCI.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutCI.setLayoutCount = 1;
    pipelineLayoutCI.pSetLayouts = &descriptorSetLayout_;

    VK_CHECK(vkCreatePipelineLayout(device_->get(), &pipelineLayoutCI, nullptr, &pipelineLayout_));
}

void RendererPost::createPipeline(VkFormat colorFormat)
{
    VkShaderModule vertexModule = device_->createShaderModule("./shaders/post_process.vert.spv");
    VkShaderModule fragmentModule = device_->createShaderModule("./shaders/post_process.frag.spv");

    std::array<VkPipelineShaderStageCreateInfo, 2> shaderSCIs{};
    shaderSCIs[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderSCIs[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    shaderSCIs[0].module = vertexModule;
    shaderSCIs[0].pName = "main";

    shaderSCIs[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderSCIs[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shaderSCIs[1].module = fragmentModule;
    shaderSCIs[1].pName = "main";

    VkPipelineVertexInputStateCreateInfo vertexInputSCI{
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};

    VkPipelineInputAssemblyStateCreateInfo inputAssemblySCI{};
    inputAssemblySCI.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssemblySCI.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssemblySCI.primitiveRestartEnable = VK_FALSE;

    VkPipelineViewportStateCreateInfo viewportSCI{};
    viewportSCI.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportSCI.viewportCount = 1;
    viewportSCI.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizationSCI{};
    rasterizationSCI.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizationSCI.depthClampEnable = VK_FALSE;
    rasterizationSCI.rasterizerDiscardEnable = VK_FALSE;
    rasterizationSCI.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizationSCI.cullMode = VK_CULL_MODE_NONE;
    rasterizationSCI.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizationSCI.depthBiasEnable = VK_FALSE;
    rasterizationSCI.lineWidth = 1.f;

    VkPipelineMultisampleStateCreateInfo multisampleSCI{};
    multisampleSCI.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampleSCI.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampleSCI.sampleShadingEnable = VK_FALSE;

    VkPipelineDepthStencilStateCreateInfo depthStencilSCI{};
    depthStencilSCI.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencilSCI.depthTestEnable = VK_FALSE;
    depthStencilSCI.depthWriteEnable = VK_FALSE;
    depthStencilSCI.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    depthStencilSCI.depthBoundsTestEnable = VK_FALSE;
    depthStencilSCI.stencilTestEnable = VK_FALSE;
    depthStencilSCI.front.failOp = VK_STENCIL_OP_KEEP;
    depthStencilSCI.front.passOp = VK_STENCIL_OP_KEEP;
    depthStencilSCI.front.depthFailOp = VK_STENCIL_OP_KEEP;
    depthStencilSCI.front.compareOp = VK_COMPARE_OP_ALWAYS;
    depthStencilSCI.front.compareMask = 0;
    depthStencilSCI.front.writeMask = 0;
    depthStencilSCI.front.reference = 0;
    depthStencilSCI.back = depthStencilSCI.front;
    depthStencilSCI.minDepthBounds = 0.f;
    depthStencilSCI.maxDepthBounds = 1.f;

    VkPipelineColorBlendAttachmentState colorBlendAttachmentState{};
    colorBlendAttachmentState.blendEnable = VK_FALSE;
    colorBlendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachmentState.alphaBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachmentState.colorWriteMask = 0xf;

    VkPipelineColorBlendStateCreateInfo colorBlendSCI{};
    colorBlendSCI.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlendSCI.logicOpEnable = VK_FALSE;
    colorBlendSCI.logicOp = VK_LOGIC_OP_COPY;
    colorBlendSCI.attachmentCount = 1;
    colorBlendSCI.pAttachments = &colorBlendAttachmentState;
    colorBlendSCI.blendConstants[0] = 0.f;
    colorBlendSCI.blendConstants[1] = 0.f;
    colorBlendSCI.blendConstants[2] = 0.f;
    colorBlendSCI.blendConstants[3] = 0.f;

    std::vector<VkDynamicState> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT,
                                                 VK_DYNAMIC_STATE_SCISSOR};

    VkPipelineDynamicStateCreateInfo dynamicSCI{};
    dynamicSCI.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicSCI.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicSCI.pDynamicStates = dynamicStates.data();

    VkPipelineRenderingCreateInfo renderingCI{};
    renderingCI.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    renderingCI.colorAttachmentCount = 1;
    renderingCI.pColorAttachmentFormats = &colorFormat;
    renderingCI.depthAttachmentFormat = device_->depthStencilFormat();
    renderingCI.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;

    VkGraphicsPipelineCreateInfo pipelineCI{};
    pipelineCI.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineCI.pNext = &renderingCI;
    pipelineCI.stageCount = static_cast<uint32_t>(shaderSCIs.size());
    pipelineCI.pStages = shaderSCIs.data();
    pipelineCI.pVertexInputState = &vertexInputSCI;
    pipelineCI.pInputAssemblyState = &inputAssemblySCI;
    pipelineCI.pViewportState = &viewportSCI;
    pipelineCI.pRasterizationState = &rasterizationSCI;
    pipelineCI.pMultisampleState = &multisampleSCI;
    pipelineCI.pDepthStencilState = &depthStencilSCI;
    pipelineCI.pColorBlendState = &colorBlendSCI;
    pipelineCI.pDynamicState = &dynamicSCI;
    pipelineCI.layout = pipelineLayout_;
    pipelineCI.renderPass = VK_NULL_HANDLE;
    pipelineCI.subpass = 0;
    pipelineCI.basePipelineHandle = VK_NULL_HANDLE;
    pipelineCI.basePipelineIndex = -1;

    VK_CHECK(vkCreateGraphicsPipelines(device_->get(), device_->cache(), 1, &pipelineCI, nullptr,
                                       &pipeline_));

    vkDestroyShaderModule(device_->get(), vertexModule, nullptr);
    vkDestroyShaderModule(device_->get(), fragmentModule, nullptr);
}

} // namespace guk