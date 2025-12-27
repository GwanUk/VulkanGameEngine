#include "RendererPost.h"
#include "Logger.h"

namespace guk {

RendererPost::RendererPost(std::shared_ptr<Device> device, VkFormat colorFormat, uint32_t width,
                           uint32_t height, std::shared_ptr<Image2D> sceneTexture)
    : device_(device), sceneTexture_(sceneTexture), bloomImage_(std::make_unique<Image2D>(device_)),
      postUniformBuffers_(std::make_unique<Buffer>(device_))
{
    createBloomImage(width, height);
    createUniform();

    createDescriptorSetLayout();
    allocateDescriptorSets();
    updateSampelrDescriptorSet();

    createPipelineLayout();
    createPipeline(colorFormat);
    createPipelineBloomDown();
    createPipelineBloomUp();
}

RendererPost::~RendererPost()
{
    vkDestroyPipeline(device_->get(), pipelineBloomUp_, nullptr);
    vkDestroyPipeline(device_->get(), pipelineBloomDown_, nullptr);
    vkDestroyPipeline(device_->get(), pipeline_, nullptr);

    vkDestroyPipelineLayout(device_->get(), pipelineLayout_, nullptr);

    vkDestroyDescriptorSetLayout(device_->get(), samplerSetLayout_, nullptr);
    vkDestroyDescriptorSetLayout(device_->get(), uniformSetLayout_, nullptr);
}

void RendererPost::resized(uint32_t width, uint32_t height)
{
    createBloomImage(width, height);
    updateSampelrDescriptorSet();
}

void RendererPost::updatePostUniform(uint32_t frameIdx, PostUniform postUniform)
{
    postUniformBuffers_[frameIdx]->update(postUniform);
}

void RendererPost::draw(VkCommandBuffer cmd, uint32_t frameIdx,
                        std::shared_ptr<Image2D> swapchainImg)
{
    sceneTexture_->transition(cmd, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                              VK_ACCESS_2_SHADER_READ_BIT,
                              VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    bloomDown(cmd);
    bloomUp(cmd);

    bloomTextures_[0]->transition(cmd, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
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

    std::array<VkDescriptorSet, 3> sets{uniformSets_[frameIdx], bloomSamplerSets_[0],
                                        sceneSamplerSet_};
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout_, 0,
                            static_cast<uint32_t>(sets.size()), sets.data(), 0, nullptr);

    vkCmdDraw(cmd, 6, 1, 0, 0);

    vkCmdEndRendering(cmd);
}

void RendererPost::bloomDown(VkCommandBuffer cmd)
{
    for (uint32_t i = 1; i < BLOOM_LEVELS; i++) {
        bloomTextures_[i - 1]->transition(cmd, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                                          VK_ACCESS_2_SHADER_READ_BIT,
                                          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        bloomTextures_[i]->transition(cmd, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                                      VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                                      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

        VkRenderingAttachmentInfo colorAttachment{};
        colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        colorAttachment.imageView = bloomTextures_[i]->view();
        colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.clearValue.color = {0.f, 0.f, 0.f, 1.f};

        VkRenderingInfo renderingInfo{};
        renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
        renderingInfo.renderArea = {0, 0, bloomTextures_[i]->width(), bloomTextures_[i]->height()};
        renderingInfo.layerCount = 1;
        renderingInfo.colorAttachmentCount = 1;
        renderingInfo.pColorAttachments = &colorAttachment;

        vkCmdBeginRendering(cmd, &renderingInfo);

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineBloomDown_);

        VkViewport viewport{};
        viewport.x = 0.f;
        viewport.y = 0.f;
        viewport.width = static_cast<float>(bloomTextures_[i]->width());
        viewport.height = static_cast<float>(bloomTextures_[i]->height());
        viewport.minDepth = 0.f;
        viewport.maxDepth = 1.f;
        vkCmdSetViewport(cmd, 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = {bloomTextures_[i]->width(), bloomTextures_[i]->height()};
        vkCmdSetScissor(cmd, 0, 1, &scissor);

        VkDescriptorSet set = i == 1 ? sceneSamplerSet_ : bloomSamplerSets_[i - 1];
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout_, 1, 1, &set,
                                0, nullptr);

        BloomPushConstants pc{};
        pc.width = static_cast<float>(bloomTextures_[i]->width());
        pc.height = static_cast<float>(bloomTextures_[i]->height());
        vkCmdPushConstants(cmd, pipelineLayout_, VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                           sizeof(BloomPushConstants), &pc);

        vkCmdDraw(cmd, 6, 1, 0, 0);

        vkCmdEndRendering(cmd);
    }
}

void RendererPost::bloomUp(VkCommandBuffer cmd)
{
    for (uint32_t i = 0; i < BLOOM_LEVELS - 1; i++) {
        uint32_t l = BLOOM_LEVELS - 2 - i;

        bloomTextures_[l + 1]->transition(cmd, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                                          VK_ACCESS_2_SHADER_READ_BIT,
                                          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        bloomTextures_[l]->transition(cmd, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                                      VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                                      VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

        VkRenderingAttachmentInfo colorAttachment{};
        colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        colorAttachment.imageView = bloomTextures_[l]->view();
        colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.clearValue.color = {0.f, 0.f, 0.f, 1.f};

        VkRenderingInfo renderingInfo{};
        renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
        renderingInfo.renderArea = {0, 0, bloomTextures_[l]->width(), bloomTextures_[l]->height()};
        renderingInfo.layerCount = 1;
        renderingInfo.colorAttachmentCount = 1;
        renderingInfo.pColorAttachments = &colorAttachment;

        vkCmdBeginRendering(cmd, &renderingInfo);

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineBloomUp_);

        VkViewport viewport{};
        viewport.x = 0.f;
        viewport.y = 0.f;
        viewport.width = static_cast<float>(bloomTextures_[l]->width());
        viewport.height = static_cast<float>(bloomTextures_[l]->height());
        viewport.minDepth = 0.f;
        viewport.maxDepth = 1.f;
        vkCmdSetViewport(cmd, 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = {bloomTextures_[l]->width(), bloomTextures_[l]->height()};
        vkCmdSetScissor(cmd, 0, 1, &scissor);

        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout_, 1, 1,
                                &bloomSamplerSets_[l + 1], 0, nullptr);

        BloomPushConstants pc{};
        pc.width = static_cast<float>(bloomTextures_[l]->width());
        pc.height = static_cast<float>(bloomTextures_[l]->height());
        vkCmdPushConstants(cmd, pipelineLayout_, VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                           sizeof(BloomPushConstants), &pc);

        vkCmdDraw(cmd, 6, 1, 0, 0);

        vkCmdEndRendering(cmd);
    }
}

void RendererPost::createBloomImage(uint32_t width, uint32_t height)
{
    bloomImage_->createImage(sceneTexture_->format(), width, height,
                             VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                             VK_SAMPLE_COUNT_1_BIT, 0, BLOOM_LEVELS);

    for (uint32_t i = 0; i < BLOOM_LEVELS; i++) {
        bloomTextures_[i] = std::make_unique<Image2D>(device_);
        bloomTextures_[i]->createView(bloomImage_->get(), bloomImage_->format(), width >> i,
                                      height >> i, i, 1);
        bloomTextures_[i]->setSampler(device_->samplerLinearClamp());
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
    VkDescriptorSetLayoutBinding uniformLayoutBindings{};
    uniformLayoutBindings.binding = 0;
    uniformLayoutBindings.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uniformLayoutBindings.descriptorCount = 1;
    uniformLayoutBindings.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo descSetLayoutCI{};
    descSetLayoutCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descSetLayoutCI.bindingCount = 1;
    descSetLayoutCI.pBindings = &uniformLayoutBindings;

    VK_CHECK(
        vkCreateDescriptorSetLayout(device_->get(), &descSetLayoutCI, nullptr, &uniformSetLayout_));

    VkDescriptorSetLayoutBinding samplerLayoutBinding{};
    samplerLayoutBinding.binding = 0;
    samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerLayoutBinding.descriptorCount = 1;
    samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    descSetLayoutCI.bindingCount = 1;
    descSetLayoutCI.pBindings = &samplerLayoutBinding;

    VK_CHECK(
        vkCreateDescriptorSetLayout(device_->get(), &descSetLayoutCI, nullptr, &samplerSetLayout_));
}

void RendererPost::allocateDescriptorSets()
{
    std::vector<VkDescriptorSetLayout> uniformLayouts(Device::MAX_FRAMES_IN_FLIGHT,
                                                      uniformSetLayout_);
    VkDescriptorSetAllocateInfo descSetAI{};
    descSetAI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descSetAI.descriptorPool = device_->descriptorPool();
    descSetAI.descriptorSetCount = static_cast<uint32_t>(uniformLayouts.size());
    descSetAI.pSetLayouts = uniformLayouts.data();

    VK_CHECK(vkAllocateDescriptorSets(device_->get(), &descSetAI, uniformSets_.data()));

    for (size_t i = 0; i < Device::MAX_FRAMES_IN_FLIGHT; i++) {
        VkDescriptorBufferInfo sceneUniformInfo{};
        sceneUniformInfo.buffer = postUniformBuffers_[i]->get();
        sceneUniformInfo.range = sizeof(PostUniform);

        VkWriteDescriptorSet writeUniform{};
        writeUniform.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeUniform.dstSet = uniformSets_[i];
        writeUniform.dstBinding = 0;
        writeUniform.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        writeUniform.descriptorCount = 1;
        writeUniform.pBufferInfo = &sceneUniformInfo;

        vkUpdateDescriptorSets(device_->get(), 1, &writeUniform, 0, nullptr);
    }

    std::vector<VkDescriptorSetLayout> bloomSamplerLayouts(BLOOM_LEVELS, samplerSetLayout_);
    descSetAI.descriptorSetCount = static_cast<uint32_t>(bloomSamplerLayouts.size());
    descSetAI.pSetLayouts = bloomSamplerLayouts.data();

    VK_CHECK(vkAllocateDescriptorSets(device_->get(), &descSetAI, bloomSamplerSets_.data()));

    descSetAI.descriptorSetCount = 1;
    descSetAI.pSetLayouts = &samplerSetLayout_;

    VK_CHECK(vkAllocateDescriptorSets(device_->get(), &descSetAI, &sceneSamplerSet_));
}

void RendererPost::updateSampelrDescriptorSet()
{
    // bloom sampler texture
    for (size_t i = 0; i < BLOOM_LEVELS; i++) {
        VkDescriptorImageInfo bloomSamplerInfo{};
        bloomSamplerInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        bloomSamplerInfo.imageView = bloomTextures_[i]->view();
        bloomSamplerInfo.sampler = bloomTextures_[i]->sampler();

        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = bloomSamplerSets_[i];
        write.dstBinding = 0;
        write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        write.descriptorCount = 1;
        write.pImageInfo = &bloomSamplerInfo;

        vkUpdateDescriptorSets(device_->get(), 1, &write, 0, nullptr);
    }

    // scene sampler texture
    VkDescriptorImageInfo sceneSamplerInfo{};
    sceneSamplerInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    sceneSamplerInfo.imageView = sceneTexture_->view();
    sceneSamplerInfo.sampler = sceneTexture_->sampler();

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = sceneSamplerSet_;
    write.dstBinding = 0;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.descriptorCount = 1;
    write.pImageInfo = &sceneSamplerInfo;
    vkUpdateDescriptorSets(device_->get(), 1, &write, 0, nullptr);
}

void RendererPost::createPipelineLayout()
{
    std::array<VkDescriptorSetLayout, 3> descriptorSetLayouts{uniformSetLayout_, samplerSetLayout_,
                                                              samplerSetLayout_};

    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(BloomPushConstants);

    VkPipelineLayoutCreateInfo pipelineLayoutCI{};
    pipelineLayoutCI.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutCI.setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size());
    pipelineLayoutCI.pSetLayouts = descriptorSetLayouts.data();
    pipelineLayoutCI.pushConstantRangeCount = 1;
    pipelineLayoutCI.pPushConstantRanges = &pushConstantRange;

    VK_CHECK(vkCreatePipelineLayout(device_->get(), &pipelineLayoutCI, nullptr, &pipelineLayout_));
}

void RendererPost::createPipelineBloomDown()
{
    VkShaderModule vertexModule = device_->createShaderModule("./shaders/post_process.vert.spv");
    VkShaderModule fragmentModule = device_->createShaderModule("./shaders/bloom_down.frag.spv");

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
    renderingCI.pColorAttachmentFormats = &sceneTexture_->format();
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
                                       &pipelineBloomDown_));

    vkDestroyShaderModule(device_->get(), vertexModule, nullptr);
    vkDestroyShaderModule(device_->get(), fragmentModule, nullptr);
}

void RendererPost::createPipelineBloomUp()
{
    VkShaderModule vertexModule = device_->createShaderModule("./shaders/post_process.vert.spv");
    VkShaderModule fragmentModule = device_->createShaderModule("./shaders/bloom_up.frag.spv");

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
    renderingCI.pColorAttachmentFormats = &sceneTexture_->format();
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
                                       &pipelineBloomUp_));

    vkDestroyShaderModule(device_->get(), vertexModule, nullptr);
    vkDestroyShaderModule(device_->get(), fragmentModule, nullptr);
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