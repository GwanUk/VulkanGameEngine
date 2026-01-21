#include "Renderer.h"
#include "Logger.h"

namespace guk {

Renderer::Renderer(std::shared_ptr<Device> device, uint32_t width, uint32_t height)
    : device_(device), msaaColorAttachment_(std::make_unique<Image2D>(device_)),
      colorAttachment_(std::make_unique<Image2D>(device_)),
      msaaDepthStencilAttachment_(std::make_unique<Image2D>(device_)),
      depthStencilAttachment_(std::make_unique<Image2D>(device_)),
      dummyTexture_(std::make_shared<Image2D>(device_))
{
    createAttachments(width, height);
    createUniform();
    createTextures();

    createDescriptorSetLayout();
    allocateDescriptorSets();

    createPipelineLayout();
    createPipeline();
    createPipelineSkybox();
}

Renderer::~Renderer()
{
    vkDestroyPipeline(device_->get(), pipelineSkybox_, nullptr);
    vkDestroyPipeline(device_->get(), pipeline_, nullptr);
    vkDestroyPipelineLayout(device_->get(), pipelineLayout_, nullptr);

    for (const auto& descriptorSetLayout : descriptorSetLayouts_) {
        vkDestroyDescriptorSetLayout(device_->get(), descriptorSetLayout, nullptr);
    }
}

void Renderer::allocateModelDescriptorSets(std::vector<Model>& models)
{
    for (auto& model : models) {
        model.allocateDescriptorSets(descriptorSetLayouts_[2], dummyTexture_);
    }
}

void Renderer::createAttachments(uint32_t width, uint32_t height)
{
    msaaColorAttachment_->createImage(VK_FORMAT_R16G16B16A16_SFLOAT, width, height,
                                      VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT |
                                          VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                                      device_->smapleCount());

    colorAttachment_->createImage(VK_FORMAT_R16G16B16A16_SFLOAT, width, height,
                                  VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                                  VK_SAMPLE_COUNT_1_BIT);
    colorAttachment_->setSampler(device_->samplerLinearClamp());

    msaaDepthStencilAttachment_->createImage(device_->depthStencilFormat(), width, height,
                                             VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT |
                                                 VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                                             device_->smapleCount());

    depthStencilAttachment_->createImage(device_->depthStencilFormat(), width, height,
                                         VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                                         VK_SAMPLE_COUNT_1_BIT);
}

void Renderer::updateScene(uint32_t frameIdx, SceneUniform sceneUniform)
{
    sceneUniformBuffers_[frameIdx]->update(sceneUniform);
}

void Renderer::updateSkybox(uint32_t frameIdx, SkyboxUniform skyboxUniform)
{
    skyboxUniformBuffers_[frameIdx]->update(skyboxUniform);
}

void Renderer::draw(VkCommandBuffer cmd, uint32_t frameIdx, std::vector<Model> models)
{
    std::vector<VkImageMemoryBarrier2> imageBarriers{
        msaaColorAttachment_->barrier2(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                                       VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                                       VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL),
        colorAttachment_->barrier2(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                                   VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                                   VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)};

    VkDependencyInfo dependencyInfo{};
    dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dependencyInfo.imageMemoryBarrierCount = static_cast<uint32_t>(imageBarriers.size());
    dependencyInfo.pImageMemoryBarriers = imageBarriers.data();

    vkCmdPipelineBarrier2(cmd, &dependencyInfo);

    VkRenderingAttachmentInfo colorAttachment{};
    colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    colorAttachment.imageView = msaaColorAttachment_->view();
    colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.clearValue.color = {0.f, 0.f, 0.f, 1.f};
    colorAttachment.resolveMode = VK_RESOLVE_MODE_AVERAGE_BIT;
    colorAttachment.resolveImageView = colorAttachment_->view();
    colorAttachment.resolveImageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkRenderingAttachmentInfo depthStecnilAttachment{};
    depthStecnilAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    depthStecnilAttachment.imageView = msaaDepthStencilAttachment_->view();
    depthStecnilAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    depthStecnilAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthStecnilAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthStecnilAttachment.clearValue.depthStencil = {1.f, 0};
    depthStecnilAttachment.resolveMode = VK_RESOLVE_MODE_SAMPLE_ZERO_BIT;
    depthStecnilAttachment.resolveImageView = depthStencilAttachment_->view();
    depthStecnilAttachment.resolveImageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkRenderingInfo renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderingInfo.renderArea = {0, 0, colorAttachment_->width(), colorAttachment_->height()};
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments = &colorAttachment;
    renderingInfo.pDepthAttachment = &depthStecnilAttachment;
    renderingInfo.pStencilAttachment = &depthStecnilAttachment;

    vkCmdBeginRendering(cmd, &renderingInfo);

    VkViewport viewport{};
    viewport.x = 0.f;
    viewport.y = 0.f;
    viewport.width = static_cast<float>(colorAttachment_->width());
    viewport.height = static_cast<float>(colorAttachment_->height());
    viewport.minDepth = 0.f;
    viewport.maxDepth = 1.f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = {colorAttachment_->width(), colorAttachment_->height()};
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    VkDeviceSize offsets[1]{0};

    // render models
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);

    for (const Model& model : models) {
        for (const Mesh& mesh : model.meshes()) {
            VkBuffer vertexBuffer = mesh.getVertexBuffer();
            VkBuffer indexBuffer = mesh.getIndexBuffer();

            std::array<VkDescriptorSet, 3> sets{uniformDescriptorSets_[frameIdx],
                                                samplerDescriptorSet_,
                                                model.getDescriptorSets(mesh.getMaterialIndex())};
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout_, 0,
                                    static_cast<uint32_t>(sets.size()), sets.data(), 0, nullptr);

            vkCmdBindVertexBuffers(cmd, 0, 1, &vertexBuffer, offsets);
            vkCmdBindIndexBuffer(cmd, indexBuffer, 0, VK_INDEX_TYPE_UINT32);
            vkCmdDrawIndexed(cmd, mesh.indicesSize(), 1, 0, 0, 0);
        }
    }

    // render skybox
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineSkybox_);

    std::array<VkDescriptorSet, 2> sets{uniformDescriptorSets_[frameIdx], samplerDescriptorSet_};
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout_, 0,
                            static_cast<uint32_t>(sets.size()), sets.data(), 0, nullptr);

    vkCmdDraw(cmd, 36, 1, 0, 0);

    vkCmdEndRendering(cmd);
}

std::shared_ptr<Image2D> Renderer::colorAttachment() const
{
    return colorAttachment_;
}

void Renderer::createUniform()
{
    for (uint32_t i = 0; i < Device::MAX_FRAMES_IN_FLIGHT; i++) {
        sceneUniformBuffers_[i] = std::make_unique<Buffer>(device_);
        sceneUniformBuffers_[i]->createUniformBuffer(sizeof(SceneUniform));

        skyboxUniformBuffers_[i] = std::make_unique<Buffer>(device_);
        skyboxUniformBuffers_[i]->createUniformBuffer(sizeof(SkyboxUniform));
    }
}

void Renderer::createTextures()
{
    dummyTexture_->createTexture("assets\\blender_uv_grid_2k.png", false);
    dummyTexture_->setSampler(device_->samplerLinearRepeat());

    std::string path = "C:\\uk_dir\\resources\\ibl_ktx2\\radkow_lake\\";

    skyboxTextures_[0] = std::make_unique<Image2D>(device_);
    skyboxTextures_[0]->createTextureKtx2(path + "specular_out.ktx2", true);
    skyboxTextures_[0]->setSampler(device_->samplerLinearRepeat());

    skyboxTextures_[1] = std::make_unique<Image2D>(device_);
    skyboxTextures_[1]->createTextureKtx2(path + "diffuse_out.ktx2", true);
    skyboxTextures_[1]->setSampler(device_->samplerLinearRepeat());

    skyboxTextures_[2] = std::make_unique<Image2D>(device_);
    skyboxTextures_[2]->createTexture(path + "outputLUT.png", false);
    skyboxTextures_[2]->setSampler(device_->samplerLinearClamp());
}

void Renderer::createDescriptorSetLayout()
{
    std::array<VkDescriptorSetLayoutBinding, 2> uniformLayoutBindings{};
    uniformLayoutBindings[0].binding = 0;
    uniformLayoutBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uniformLayoutBindings[0].descriptorCount = 1;
    uniformLayoutBindings[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    uniformLayoutBindings[1].binding = 1;
    uniformLayoutBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uniformLayoutBindings[1].descriptorCount = 1;
    uniformLayoutBindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo descSetLayoutCI{
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    descSetLayoutCI.bindingCount = static_cast<uint32_t>(uniformLayoutBindings.size());
    descSetLayoutCI.pBindings = uniformLayoutBindings.data();

    VK_CHECK(vkCreateDescriptorSetLayout(device_->get(), &descSetLayoutCI, nullptr,
                                         &descriptorSetLayouts_[0]));

    std::array<VkDescriptorSetLayoutBinding, 3> samplerLayoutBindings{};
    samplerLayoutBindings[0].binding = 0;
    samplerLayoutBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerLayoutBindings[0].descriptorCount = 1;
    samplerLayoutBindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    samplerLayoutBindings[1].binding = 1;
    samplerLayoutBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerLayoutBindings[1].descriptorCount = 1;
    samplerLayoutBindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    samplerLayoutBindings[2].binding = 2;
    samplerLayoutBindings[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerLayoutBindings[2].descriptorCount = 1;
    samplerLayoutBindings[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    descSetLayoutCI.bindingCount = static_cast<uint32_t>(samplerLayoutBindings.size());
    descSetLayoutCI.pBindings = samplerLayoutBindings.data();

    VK_CHECK(vkCreateDescriptorSetLayout(device_->get(), &descSetLayoutCI, nullptr,
                                         &descriptorSetLayouts_[1]));

    std::array<VkDescriptorSetLayoutBinding, 6> materialLayoutBindings{};
    materialLayoutBindings[0].binding = 0;
    materialLayoutBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    materialLayoutBindings[0].descriptorCount = 1;
    materialLayoutBindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    materialLayoutBindings[1].binding = 1;
    materialLayoutBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    materialLayoutBindings[1].descriptorCount = 1;
    materialLayoutBindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    materialLayoutBindings[2].binding = 2;
    materialLayoutBindings[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    materialLayoutBindings[2].descriptorCount = 1;
    materialLayoutBindings[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    materialLayoutBindings[3].binding = 3;
    materialLayoutBindings[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    materialLayoutBindings[3].descriptorCount = 1;
    materialLayoutBindings[3].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    materialLayoutBindings[4].binding = 4;
    materialLayoutBindings[4].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    materialLayoutBindings[4].descriptorCount = 1;
    materialLayoutBindings[4].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    materialLayoutBindings[5].binding = 5;
    materialLayoutBindings[5].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    materialLayoutBindings[5].descriptorCount = 1;
    materialLayoutBindings[5].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    descSetLayoutCI.bindingCount = static_cast<uint32_t>(materialLayoutBindings.size());
    descSetLayoutCI.pBindings = materialLayoutBindings.data();

    VK_CHECK(vkCreateDescriptorSetLayout(device_->get(), &descSetLayoutCI, nullptr,
                                         &descriptorSetLayouts_[2]));
}

void Renderer::allocateDescriptorSets()
{
    // uniform buffer
    std::vector<VkDescriptorSetLayout> uniformLayouts(Device::MAX_FRAMES_IN_FLIGHT,
                                                      descriptorSetLayouts_[0]);
    VkDescriptorSetAllocateInfo descSetAI{};
    descSetAI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descSetAI.descriptorPool = device_->descriptorPool();
    descSetAI.descriptorSetCount = static_cast<uint32_t>(uniformLayouts.size());
    descSetAI.pSetLayouts = uniformLayouts.data();

    VK_CHECK(vkAllocateDescriptorSets(device_->get(), &descSetAI, uniformDescriptorSets_.data()));

    for (size_t i = 0; i < Device::MAX_FRAMES_IN_FLIGHT; i++) {
        VkDescriptorBufferInfo sceneUniformInfo{};
        sceneUniformInfo.buffer = sceneUniformBuffers_[i]->get();
        sceneUniformInfo.range = sizeof(SceneUniform);

        VkDescriptorBufferInfo skyboxUniformInfo{};
        skyboxUniformInfo.buffer = skyboxUniformBuffers_[i]->get();
        skyboxUniformInfo.range = sizeof(SkyboxUniform);

        std::array<VkWriteDescriptorSet, 2> writeUniform{};
        writeUniform[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeUniform[0].dstSet = uniformDescriptorSets_[i];
        writeUniform[0].dstBinding = 0;
        writeUniform[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        writeUniform[0].descriptorCount = 1;
        writeUniform[0].pBufferInfo = &sceneUniformInfo;

        writeUniform[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writeUniform[1].dstSet = uniformDescriptorSets_[i];
        writeUniform[1].dstBinding = 1;
        writeUniform[1].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        writeUniform[1].descriptorCount = 1;
        writeUniform[1].pBufferInfo = &skyboxUniformInfo;

        vkUpdateDescriptorSets(device_->get(), static_cast<uint32_t>(writeUniform.size()),
                               writeUniform.data(), 0, nullptr);
    }

    // texture sampler
    descSetAI.descriptorSetCount = 1;
    descSetAI.pSetLayouts = &descriptorSetLayouts_[1];

    VK_CHECK(vkAllocateDescriptorSets(device_->get(), &descSetAI, &samplerDescriptorSet_));

    VkDescriptorImageInfo prefilteredInfo{};
    prefilteredInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    prefilteredInfo.imageView = skyboxTextures_[0]->view();
    prefilteredInfo.sampler = skyboxTextures_[0]->sampler();

    VkDescriptorImageInfo irradianceInfo{};
    irradianceInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    irradianceInfo.imageView = skyboxTextures_[1]->view();
    irradianceInfo.sampler = skyboxTextures_[1]->sampler();

    VkDescriptorImageInfo brdfLutInfo{};
    brdfLutInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    brdfLutInfo.imageView = skyboxTextures_[2]->view();
    brdfLutInfo.sampler = skyboxTextures_[2]->sampler();

    std::array<VkWriteDescriptorSet, 3> writeSampler{};
    writeSampler[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeSampler[0].dstSet = samplerDescriptorSet_;
    writeSampler[0].dstBinding = 0;
    writeSampler[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writeSampler[0].descriptorCount = 1;
    writeSampler[0].pImageInfo = &prefilteredInfo;

    writeSampler[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeSampler[1].dstSet = samplerDescriptorSet_;
    writeSampler[1].dstBinding = 1;
    writeSampler[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writeSampler[1].descriptorCount = 1;
    writeSampler[1].pImageInfo = &irradianceInfo;

    writeSampler[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writeSampler[2].dstSet = samplerDescriptorSet_;
    writeSampler[2].dstBinding = 2;
    writeSampler[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writeSampler[2].descriptorCount = 1;
    writeSampler[2].pImageInfo = &brdfLutInfo;

    vkUpdateDescriptorSets(device_->get(), static_cast<uint32_t>(writeSampler.size()),
                           writeSampler.data(), 0, nullptr);
}

void Renderer::createPipelineLayout()
{
    VkPipelineLayoutCreateInfo pipelineLayoutCI{};
    pipelineLayoutCI.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutCI.setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts_.size());
    pipelineLayoutCI.pSetLayouts = descriptorSetLayouts_.data();

    VK_CHECK(vkCreatePipelineLayout(device_->get(), &pipelineLayoutCI, nullptr, &pipelineLayout_));
}

void Renderer::createPipeline()
{
    VkShaderModule vertexModule = device_->createShaderModule("./shaders/pbr.vert.spv");
    VkShaderModule fragmentModule = device_->createShaderModule("./shaders/pbr.frag.spv");

    std::array<VkPipelineShaderStageCreateInfo, 2> shaderSCIs{};
    shaderSCIs[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderSCIs[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    shaderSCIs[0].module = vertexModule;
    shaderSCIs[0].pName = "main";

    shaderSCIs[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderSCIs[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shaderSCIs[1].module = fragmentModule;
    shaderSCIs[1].pName = "main";

    auto bindingDescription = Vertex::getBindingDescrption();
    auto attributeDescriptions = Vertex::getAttributeDescriptions();

    VkPipelineVertexInputStateCreateInfo vertexInputSCI{};
    vertexInputSCI.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputSCI.vertexBindingDescriptionCount = 1;
    vertexInputSCI.pVertexBindingDescriptions = &bindingDescription;
    vertexInputSCI.vertexAttributeDescriptionCount =
        static_cast<uint32_t>(attributeDescriptions.size());
    vertexInputSCI.pVertexAttributeDescriptions = attributeDescriptions.data();

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
    rasterizationSCI.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizationSCI.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterizationSCI.depthBiasEnable = VK_FALSE;
    rasterizationSCI.lineWidth = 1.f;

    VkPipelineMultisampleStateCreateInfo multisampleSCI{};
    multisampleSCI.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampleSCI.rasterizationSamples = device_->smapleCount();
    multisampleSCI.sampleShadingEnable = VK_FALSE;

    VkPipelineDepthStencilStateCreateInfo depthStencilSCI{};
    depthStencilSCI.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencilSCI.depthTestEnable = VK_TRUE;
    depthStencilSCI.depthWriteEnable = VK_TRUE;
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
    renderingCI.pColorAttachmentFormats = &colorAttachment_->format();
    renderingCI.depthAttachmentFormat = device_->depthStencilFormat();
    renderingCI.stencilAttachmentFormat = device_->depthStencilFormat();

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

void Renderer::createPipelineSkybox()
{
    VkShaderModule vertexModule = device_->createShaderModule("./shaders/skybox.vert.spv");
    VkShaderModule fragmentModule = device_->createShaderModule("./shaders/skybox.frag.spv");

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
    multisampleSCI.rasterizationSamples = device_->smapleCount();
    multisampleSCI.sampleShadingEnable = VK_FALSE;

    VkPipelineDepthStencilStateCreateInfo depthStencilSCI{};
    depthStencilSCI.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencilSCI.depthTestEnable = VK_TRUE;
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
    renderingCI.pColorAttachmentFormats = &colorAttachment_->format();
    renderingCI.depthAttachmentFormat = device_->depthStencilFormat();
    renderingCI.stencilAttachmentFormat = device_->depthStencilFormat();

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
                                       &pipelineSkybox_));

    vkDestroyShaderModule(device_->get(), vertexModule, nullptr);
    vkDestroyShaderModule(device_->get(), fragmentModule, nullptr);
}

} // namespace guk