#include "SceneRenderer.h"

#include <glm/gtc/matrix_transform.hpp>
#include <chrono>

namespace guk {

SceneRenderer::SceneRenderer(Engine& engine)
    : engine_(engine), colorAttahcment_{engine_}, depthStencilAttahcment_{engine_},
      textureImage_{engine_}

{
    createBuffers();
    createUniformBuffers();
    createTextures();
    createAttachments();

    createDescriptorSetLayout();
    allocateDescriptorSets();
    createPipelineLayout();
    createPipeline();
}

SceneRenderer::~SceneRenderer()
{
    for (size_t i = 0; i < engine_.MAX_FRAMES_IN_FLIGHT; i++) {
        vkUnmapMemory(engine_.device_, sceneMemory_[i]);
        vkDestroyBuffer(engine_.device_, sceneBuffers_[i], nullptr);
        vkFreeMemory(engine_.device_, sceneMemory_[i], nullptr);
    }

    vkDestroyBuffer(engine_.device_, indexBuffer_, nullptr);
    vkFreeMemory(engine_.device_, vertexMemory_, nullptr);

    vkDestroyBuffer(engine_.device_, vertexBuffer_, nullptr);
    vkFreeMemory(engine_.device_, indexMemory_, nullptr);

    vkDestroyPipeline(engine_.device_, pipeline_, nullptr);
    vkDestroyPipelineLayout(engine_.device_, pipelineLayout_, nullptr);
    vkDestroyDescriptorSetLayout(engine_.device_, descriptorSetLayout_, nullptr);
}

void SceneRenderer::createAttachments()
{
    colorAttahcment_.createImage(
        engine_.swapchainImages_[0].extent_, engine_.swapchainImages_[0].format_,
        VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        engine_.sampleCount_);
    depthStencilAttahcment_.createImage(
        engine_.swapchainImages_[0].extent_, engine_.depthStencilFormat_,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, engine_.sampleCount_);
}

void SceneRenderer::createTextures()
{
    textureImage_.createTexture("./assets/blender_uv_grid_2k.png", VK_FORMAT_R8G8B8A8_SRGB,
                                VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                                    VK_IMAGE_USAGE_SAMPLED_BIT,
                                true);
}

void SceneRenderer::updateUniform(uint32_t frameIdx)
{
    static auto startTime = std::chrono::high_resolution_clock::now();

    auto currentTime = std::chrono::high_resolution_clock::now();
    float time =
        std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

    SceneUniform ubo{};
    ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.f), glm::vec3(0.f, 0.f, 1.f));
    ubo.view =
        glm::lookAt(glm::vec3(2.f, 2.f, 2.f), glm::vec3(0.f, 0.f, 0.f), glm::vec3(0.f, 0.f, 1.f));
    ubo.proj = glm::perspective(glm::radians(45.f),
                                engine_.swapchainImages_[0].extent_.width /
                                    (float)engine_.swapchainImages_[0].extent_.height,
                                0.1f, 10.0f);
    ubo.proj[1][1] *= -1;

    memcpy(sceneMapped_[frameIdx], &ubo, sizeof(ubo));
}

void SceneRenderer::draw(VkCommandBuffer cmd, uint32_t frameIdx, Image2D& renderTarget)
{
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = 0;
    beginInfo.pInheritanceInfo = nullptr;

    VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo));

    VkImageMemoryBarrier2 colorAttahcmentBarrier{};
    colorAttahcmentBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    colorAttahcmentBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    colorAttahcmentBarrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
    colorAttahcmentBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    colorAttahcmentBarrier.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
    colorAttahcmentBarrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttahcmentBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttahcmentBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    colorAttahcmentBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    colorAttahcmentBarrier.image = colorAttahcment_.image_;
    colorAttahcmentBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    colorAttahcmentBarrier.subresourceRange.baseMipLevel = 0;
    colorAttahcmentBarrier.subresourceRange.levelCount = 1;
    colorAttahcmentBarrier.subresourceRange.baseArrayLayer = 0;
    colorAttahcmentBarrier.subresourceRange.layerCount = 1;

    VkImageMemoryBarrier2 swapchainImageBarrier{};
    swapchainImageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    swapchainImageBarrier.srcStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
    swapchainImageBarrier.srcAccessMask = VK_ACCESS_2_NONE;
    swapchainImageBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    swapchainImageBarrier.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
    swapchainImageBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    swapchainImageBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    swapchainImageBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    swapchainImageBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    swapchainImageBarrier.image = renderTarget.image_;
    swapchainImageBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    swapchainImageBarrier.subresourceRange.baseMipLevel = 0;
    swapchainImageBarrier.subresourceRange.levelCount = 1;
    swapchainImageBarrier.subresourceRange.baseArrayLayer = 0;
    swapchainImageBarrier.subresourceRange.layerCount = 1;

    VkImageMemoryBarrier2 depthStencilBarrier{};
    depthStencilBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    depthStencilBarrier.srcStageMask = VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
    depthStencilBarrier.srcAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    depthStencilBarrier.dstStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT;
    depthStencilBarrier.dstAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    depthStencilBarrier.oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    depthStencilBarrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    depthStencilBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    depthStencilBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    depthStencilBarrier.image = depthStencilAttahcment_.image_;
    depthStencilBarrier.subresourceRange.aspectMask =
        VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
    depthStencilBarrier.subresourceRange.baseMipLevel = 0;
    depthStencilBarrier.subresourceRange.levelCount = 1;
    depthStencilBarrier.subresourceRange.baseArrayLayer = 0;
    depthStencilBarrier.subresourceRange.layerCount = 1;

    std::vector<VkImageMemoryBarrier2> imageBarriers{colorAttahcmentBarrier, swapchainImageBarrier,
                                                     depthStencilBarrier};
    VkDependencyInfo dependencyInfo{};
    dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dependencyInfo.imageMemoryBarrierCount = static_cast<uint32_t>(imageBarriers.size());
    dependencyInfo.pImageMemoryBarriers = imageBarriers.data();

    vkCmdPipelineBarrier2(cmd, &dependencyInfo);

    VkRenderingAttachmentInfo colorAttachment{};
    colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    colorAttachment.imageView = colorAttahcment_.view_;
    colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.clearValue.color = {0.f, 0.f, 0.f, 1.f};
    colorAttachment.resolveMode = VK_RESOLVE_MODE_AVERAGE_BIT;
    colorAttachment.resolveImageView = renderTarget.view_;
    colorAttachment.resolveImageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkRenderingAttachmentInfo depthStecilAttachment{};
    depthStecilAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    depthStecilAttachment.imageView = depthStencilAttahcment_.view_;
    depthStecilAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    depthStecilAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthStecilAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthStecilAttachment.clearValue.depthStencil = {1.f, 0};

    VkRenderingInfo renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderingInfo.renderArea = {0, 0, renderTarget.extent_.width, renderTarget.extent_.height};
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments = &colorAttachment;
    renderingInfo.pDepthAttachment = &depthStecilAttachment;

    vkCmdBeginRendering(cmd, &renderingInfo);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);

    VkViewport viewport{};
    viewport.x = 0.f;
    viewport.y = 0.f;
    viewport.width = static_cast<float>(renderTarget.extent_.width);
    viewport.height = static_cast<float>(renderTarget.extent_.height);
    viewport.minDepth = 0.f;
    viewport.maxDepth = 1.f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = renderTarget.extent_;
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    VkBuffer vertexbuffers[] = {vertexBuffer_};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(cmd, 0, 1, vertexbuffers, offsets);

    vkCmdBindIndexBuffer(cmd, indexBuffer_, 0, VK_INDEX_TYPE_UINT16);

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout_, 0, 1,
                            &descriptorSets_[frameIdx], 0, nullptr);

    vkCmdDrawIndexed(cmd, static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);

    vkCmdEndRendering(cmd);
}

void SceneRenderer::createBuffers()
{
    VkDeviceSize vertexSize = sizeof(vertices[0]) * vertices.size();
    VkDeviceSize indexSize = sizeof(indices[0]) * indices.size();

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingMemory;

    VkBufferCreateInfo bufferCI{};
    bufferCI.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferCI.size = vertexSize + indexSize;
    bufferCI.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bufferCI.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VK_CHECK(vkCreateBuffer(engine_.device_, &bufferCI, nullptr, &stagingBuffer));

    VkMemoryRequirements memoryRs;
    vkGetBufferMemoryRequirements(engine_.device_, stagingBuffer, &memoryRs);

    VkMemoryAllocateInfo memoryAI{};
    memoryAI.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memoryAI.allocationSize = memoryRs.size;
    memoryAI.memoryTypeIndex = engine_.getMemoryTypeIndex(memoryRs.memoryTypeBits,
                                                          VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                              VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    VK_CHECK(vkAllocateMemory(engine_.device_, &memoryAI, nullptr, &stagingMemory));
    VK_CHECK(vkBindBufferMemory(engine_.device_, stagingBuffer, stagingMemory, 0));

    void* mapped;
    VK_CHECK(vkMapMemory(engine_.device_, stagingMemory, 0, vertexSize + indexSize, 0, &mapped));
    memcpy(mapped, vertices.data(), static_cast<size_t>(vertexSize));
    memcpy(static_cast<char*>(mapped) + vertexSize, indices.data(), static_cast<size_t>(indexSize));
    vkUnmapMemory(engine_.device_, stagingMemory);

    bufferCI.size = vertexSize;
    bufferCI.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    VK_CHECK(vkCreateBuffer(engine_.device_, &bufferCI, nullptr, &vertexBuffer_));

    vkGetBufferMemoryRequirements(engine_.device_, vertexBuffer_, &memoryRs);
    memoryAI.allocationSize = memoryRs.size;
    memoryAI.memoryTypeIndex =
        engine_.getMemoryTypeIndex(memoryRs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VK_CHECK(vkAllocateMemory(engine_.device_, &memoryAI, nullptr, &vertexMemory_));
    VK_CHECK(vkBindBufferMemory(engine_.device_, vertexBuffer_, vertexMemory_, 0));

    bufferCI.size = indexSize;
    bufferCI.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    VK_CHECK(vkCreateBuffer(engine_.device_, &bufferCI, nullptr, &indexBuffer_));

    vkGetBufferMemoryRequirements(engine_.device_, indexBuffer_, &memoryRs);
    memoryAI.allocationSize = memoryRs.size;
    memoryAI.memoryTypeIndex =
        engine_.getMemoryTypeIndex(memoryRs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VK_CHECK(vkAllocateMemory(engine_.device_, &memoryAI, nullptr, &indexMemory_));
    VK_CHECK(vkBindBufferMemory(engine_.device_, indexBuffer_, indexMemory_, 0));

    VkCommandBuffer cmd = engine_.beginCommand();

    VkBufferCopy copy{};
    copy.size = vertexSize;
    vkCmdCopyBuffer(cmd, stagingBuffer, vertexBuffer_, 1, &copy);

    copy.srcOffset = vertexSize;
    copy.size = indexSize;
    vkCmdCopyBuffer(cmd, stagingBuffer, indexBuffer_, 1, &copy);

    engine_.submitAndWait(cmd);

    vkDestroyBuffer(engine_.device_, stagingBuffer, nullptr);
    vkFreeMemory(engine_.device_, stagingMemory, nullptr);
}

void SceneRenderer::createUniformBuffers()
{
    for (size_t i = 0; i < Engine::MAX_FRAMES_IN_FLIGHT; i++) {
        VkBufferCreateInfo bufferCI{};
        bufferCI.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferCI.size = sizeof(SceneUniform);
        bufferCI.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        bufferCI.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VK_CHECK(vkCreateBuffer(engine_.device_, &bufferCI, nullptr, &sceneBuffers_[i]));

        VkMemoryRequirements memoryRs;
        vkGetBufferMemoryRequirements(engine_.device_, sceneBuffers_[i], &memoryRs);

        VkMemoryAllocateInfo memoryAI{};
        memoryAI.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        memoryAI.allocationSize = memoryRs.size;
        memoryAI.memoryTypeIndex = engine_.getMemoryTypeIndex(
            memoryRs.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        VK_CHECK(vkAllocateMemory(engine_.device_, &memoryAI, nullptr, &sceneMemory_[i]));
        VK_CHECK(vkBindBufferMemory(engine_.device_, sceneBuffers_[i], sceneMemory_[i], 0));

        VK_CHECK(vkMapMemory(engine_.device_, sceneMemory_[i], 0, sizeof(SceneUniform), 0,
                             &sceneMapped_[i]));
    }
}

void SceneRenderer::createDescriptorSetLayout()
{
    VkDescriptorSetLayoutBinding uboDescriptorSetLayoutBinding{};
    uboDescriptorSetLayoutBinding.binding = 0;
    uboDescriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboDescriptorSetLayoutBinding.descriptorCount = 1;
    uboDescriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorSetLayoutBinding samplerDescriptorSetLayoutBinding{};
    samplerDescriptorSetLayoutBinding.binding = 1;
    samplerDescriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerDescriptorSetLayoutBinding.descriptorCount = 1;
    samplerDescriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    std::vector<VkDescriptorSetLayoutBinding> descriptorSetLayoutBindings{
        uboDescriptorSetLayoutBinding, samplerDescriptorSetLayoutBinding};

    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCI{};
    descriptorSetLayoutCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptorSetLayoutCI.bindingCount = static_cast<uint32_t>(descriptorSetLayoutBindings.size());
    descriptorSetLayoutCI.pBindings = descriptorSetLayoutBindings.data();

    VK_CHECK(vkCreateDescriptorSetLayout(engine_.device_, &descriptorSetLayoutCI, nullptr,
                                         &descriptorSetLayout_));
}

void SceneRenderer::allocateDescriptorSets()
{
    std::vector<VkDescriptorSetLayout> descriptorSetLayouts(engine_.MAX_FRAMES_IN_FLIGHT,
                                                            descriptorSetLayout_);

    VkDescriptorSetAllocateInfo descriptorSetAllocateInfo{};
    descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descriptorSetAllocateInfo.descriptorPool = engine_.descriptorPool_;
    descriptorSetAllocateInfo.descriptorSetCount = engine_.MAX_FRAMES_IN_FLIGHT;
    descriptorSetAllocateInfo.pSetLayouts = descriptorSetLayouts.data();

    VK_CHECK(vkAllocateDescriptorSets(engine_.device_, &descriptorSetAllocateInfo,
                                      descriptorSets_.data()));

    for (uint32_t i = 0; i < engine_.MAX_FRAMES_IN_FLIGHT; i++) {
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = sceneBuffers_[i];
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(SceneUniform);

        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = textureImage_.view_;
        imageInfo.sampler = textureImage_.sampler_;

        std::vector<VkWriteDescriptorSet> descriptorWrites(2);

        descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].dstSet = descriptorSets_[i];
        descriptorWrites[0].dstBinding = 0;
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].pBufferInfo = &bufferInfo;
        descriptorWrites[0].pImageInfo = nullptr;
        descriptorWrites[0].pTexelBufferView = nullptr;

        descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[1].dstSet = descriptorSets_[i];
        descriptorWrites[1].dstBinding = 1;
        descriptorWrites[1].dstArrayElement = 0;
        descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrites[1].descriptorCount = 1;
        descriptorWrites[1].pBufferInfo = nullptr;
        descriptorWrites[1].pImageInfo = &imageInfo;
        descriptorWrites[1].pTexelBufferView = nullptr;

        vkUpdateDescriptorSets(engine_.device_, static_cast<uint32_t>(descriptorWrites.size()),
                               descriptorWrites.data(), 0, nullptr);
    }
}

void SceneRenderer::createPipelineLayout()
{
    VkPipelineLayoutCreateInfo pipelineLayoutCI{};
    pipelineLayoutCI.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutCI.setLayoutCount = 1;
    pipelineLayoutCI.pSetLayouts = &descriptorSetLayout_;

    VK_CHECK(vkCreatePipelineLayout(engine_.device_, &pipelineLayoutCI, nullptr, &pipelineLayout_));
}

void SceneRenderer::createPipeline()
{
    VkShaderModule vertexShaderModule = engine_.createShaderModule("./shaders/test.vert.spv");
    VkShaderModule fragmentShaderModule = engine_.createShaderModule("./shaders/test.frag.spv");

    VkPipelineShaderStageCreateInfo pipelineVertexShaderStageCI{};
    pipelineVertexShaderStageCI.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipelineVertexShaderStageCI.stage = VK_SHADER_STAGE_VERTEX_BIT;
    pipelineVertexShaderStageCI.module = vertexShaderModule;
    pipelineVertexShaderStageCI.pName = "main";

    VkPipelineShaderStageCreateInfo pipelineFragmentShaderStageCI{};
    pipelineFragmentShaderStageCI.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipelineFragmentShaderStageCI.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    pipelineFragmentShaderStageCI.module = fragmentShaderModule;
    pipelineFragmentShaderStageCI.pName = "main";

    VkPipelineShaderStageCreateInfo pipelineShaderStageCIs[] = {pipelineVertexShaderStageCI,
                                                                pipelineFragmentShaderStageCI};

    auto vertexInputBindingDescrption = Vertex::getBindingDescrption();
    auto vertexInputAttributeDescriptions = Vertex::getAttributeDescriptions();

    VkPipelineVertexInputStateCreateInfo pipelineVertexInputStateCI{};
    pipelineVertexInputStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    pipelineVertexInputStateCI.vertexBindingDescriptionCount = 1;
    pipelineVertexInputStateCI.pVertexBindingDescriptions = &vertexInputBindingDescrption;
    pipelineVertexInputStateCI.vertexAttributeDescriptionCount =
        static_cast<uint32_t>(vertexInputAttributeDescriptions.size());
    pipelineVertexInputStateCI.pVertexAttributeDescriptions =
        vertexInputAttributeDescriptions.data();

    VkPipelineInputAssemblyStateCreateInfo pipelineInputAssemblyStateCI{};
    pipelineInputAssemblyStateCI.sType =
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    pipelineInputAssemblyStateCI.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    pipelineInputAssemblyStateCI.primitiveRestartEnable = VK_FALSE;

    VkPipelineRasterizationStateCreateInfo pipelineRasterizationStateCI{};
    pipelineRasterizationStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    pipelineRasterizationStateCI.depthClampEnable = VK_FALSE;
    pipelineRasterizationStateCI.rasterizerDiscardEnable = VK_FALSE;
    pipelineRasterizationStateCI.polygonMode = VK_POLYGON_MODE_FILL;
    pipelineRasterizationStateCI.lineWidth = 1.f;
    pipelineRasterizationStateCI.cullMode = VK_CULL_MODE_BACK_BIT;
    pipelineRasterizationStateCI.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    pipelineRasterizationStateCI.depthBiasEnable = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo pipelineMultisampleStateCI{};
    pipelineMultisampleStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    pipelineMultisampleStateCI.sampleShadingEnable = VK_FALSE;
    pipelineMultisampleStateCI.rasterizationSamples = engine_.sampleCount_;

    VkPipelineDepthStencilStateCreateInfo pipelineDepthStencilStateCI{};
    pipelineDepthStencilStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    pipelineDepthStencilStateCI.depthTestEnable = VK_TRUE;
    pipelineDepthStencilStateCI.depthWriteEnable = VK_TRUE;
    pipelineDepthStencilStateCI.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    pipelineDepthStencilStateCI.depthBoundsTestEnable = VK_FALSE;
    pipelineDepthStencilStateCI.minDepthBounds = 0.f;
    pipelineDepthStencilStateCI.maxDepthBounds = 1.f;
    pipelineDepthStencilStateCI.stencilTestEnable = VK_FALSE;

    VkPipelineColorBlendAttachmentState pipelineColorBlendAttachmentState{};
    pipelineColorBlendAttachmentState.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
        VK_COLOR_COMPONENT_A_BIT;
    pipelineColorBlendAttachmentState.blendEnable = VK_FALSE;
    pipelineColorBlendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    pipelineColorBlendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
    pipelineColorBlendAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;
    pipelineColorBlendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    pipelineColorBlendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    pipelineColorBlendAttachmentState.alphaBlendOp = VK_BLEND_OP_ADD;

    VkPipelineColorBlendStateCreateInfo pipelineColorBlendStateCI{};
    pipelineColorBlendStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    pipelineColorBlendStateCI.logicOpEnable = VK_FALSE;
    pipelineColorBlendStateCI.logicOp = VK_LOGIC_OP_COPY;
    pipelineColorBlendStateCI.attachmentCount = 1;
    pipelineColorBlendStateCI.pAttachments = &pipelineColorBlendAttachmentState;
    pipelineColorBlendStateCI.blendConstants[0] = 0.f;
    pipelineColorBlendStateCI.blendConstants[1] = 0.f;
    pipelineColorBlendStateCI.blendConstants[2] = 0.f;
    pipelineColorBlendStateCI.blendConstants[3] = 0.f;

    VkPipelineViewportStateCreateInfo pipelineViewportStateCI{};
    pipelineViewportStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    pipelineViewportStateCI.viewportCount = 1;
    pipelineViewportStateCI.scissorCount = 1;

    std::vector<VkDynamicState> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT,
                                                 VK_DYNAMIC_STATE_SCISSOR};

    VkPipelineDynamicStateCreateInfo pipelineDynamicStateCI{};
    pipelineDynamicStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    pipelineDynamicStateCI.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    pipelineDynamicStateCI.pDynamicStates = dynamicStates.data();

    VkPipelineRenderingCreateInfo pipelineRenderingCI{};
    pipelineRenderingCI.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    pipelineRenderingCI.colorAttachmentCount = 1;
    pipelineRenderingCI.pColorAttachmentFormats = &colorAttahcment_.format_;
    pipelineRenderingCI.depthAttachmentFormat = depthStencilAttahcment_.format_;
    pipelineRenderingCI.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;

    VkGraphicsPipelineCreateInfo graphicsPipelineCI{};
    graphicsPipelineCI.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    graphicsPipelineCI.pNext = &pipelineRenderingCI;
    graphicsPipelineCI.stageCount = 2;
    graphicsPipelineCI.pStages = pipelineShaderStageCIs;
    graphicsPipelineCI.pVertexInputState = &pipelineVertexInputStateCI;
    graphicsPipelineCI.pInputAssemblyState = &pipelineInputAssemblyStateCI;
    graphicsPipelineCI.pViewportState = &pipelineViewportStateCI;
    graphicsPipelineCI.pRasterizationState = &pipelineRasterizationStateCI;
    graphicsPipelineCI.pMultisampleState = &pipelineMultisampleStateCI;
    graphicsPipelineCI.pDepthStencilState = &pipelineDepthStencilStateCI;
    graphicsPipelineCI.pColorBlendState = &pipelineColorBlendStateCI;
    graphicsPipelineCI.pDynamicState = &pipelineDynamicStateCI;
    graphicsPipelineCI.layout = pipelineLayout_;
    graphicsPipelineCI.renderPass = VK_NULL_HANDLE;
    graphicsPipelineCI.subpass = 0;
    graphicsPipelineCI.basePipelineHandle = VK_NULL_HANDLE;
    graphicsPipelineCI.basePipelineIndex = -1;

    VK_CHECK(vkCreateGraphicsPipelines(engine_.device_, engine_.pipelineCache_, 1,
                                       &graphicsPipelineCI, nullptr, &pipeline_));

    vkDestroyShaderModule(engine_.device_, vertexShaderModule, nullptr);
    vkDestroyShaderModule(engine_.device_, fragmentShaderModule, nullptr);
}

} // namespace guk