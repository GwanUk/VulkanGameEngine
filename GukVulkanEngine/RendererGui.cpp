#include "RendererGui.h"
#include "Logger.h"
#include "DataStructures.h"

#include <imgui.h>

namespace guk {

RendererGui::RendererGui(std::shared_ptr<Device> device, VkFormat colorFormat)
    : device_(device), fontTexture_(std::make_unique<Image2D>(device_))
{
    init();
    createDescriptorSetLayout();
    allocateDescriptorSets();
    createPipelineLayout();
    createPipeline(colorFormat);
}

RendererGui::~RendererGui()
{
    vkDestroyPipeline(device_->get(), pipeline_, nullptr);
    vkDestroyPipelineLayout(device_->get(), pipelineLayout_, nullptr);
    vkDestroyDescriptorSetLayout(device_->get(), descriptorSetLayout_, nullptr);

    for (size_t i = 0; i < Device::MAX_FRAMES_IN_FLIGHT; i++) {
        vkUnmapMemory(device_->get(), vertexBufferMemorys_[i]);
        vkDestroyBuffer(device_->get(), vertexBuffers_[i], nullptr);
        vkFreeMemory(device_->get(), vertexBufferMemorys_[i], nullptr);

        vkUnmapMemory(device_->get(), indexBufferMemorys_[i]);
        vkDestroyBuffer(device_->get(), indexBuffers_[i], nullptr);
        vkFreeMemory(device_->get(), indexBufferMemorys_[i], nullptr);
    }
}

void RendererGui::update(uint32_t frameIdx)
{
    ImDrawData* drawData = ImGui::GetDrawData();
    if (!drawData || !drawData->TotalVtxCount || !drawData->TotalIdxCount) {
        return;
    }

    VkDeviceSize vertexBufferSize = drawData->TotalVtxCount * sizeof(ImDrawVert);
    VkDeviceSize indexBufferSize = drawData->TotalIdxCount * sizeof(ImDrawIdx);

    if (vertexBuffers_[frameIdx] != VK_NULL_HANDLE ||
        vertexBufferSize > vertexAllocationSizes_[frameIdx]) {

        VkDeviceSize newSize = std::max(static_cast<VkDeviceSize>(vertexBufferSize * 1.5f),
                                        static_cast<VkDeviceSize>(512 * sizeof(ImDrawVert)));
        createBuffer(frameIdx, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, newSize);
    }

    if (indexBuffers_[frameIdx] != VK_NULL_HANDLE ||
        indexBufferSize > indexAllocationSizes_[frameIdx]) {

        VkDeviceSize newSize = std::max(static_cast<VkDeviceSize>(vertexBufferSize * 1.5f),
                                        static_cast<VkDeviceSize>(1024 * sizeof(ImDrawIdx)));
        createBuffer(frameIdx, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, newSize);
    }

    ImDrawVert* vtxDst = (ImDrawVert*)vertexMappeds_[frameIdx];
    ImDrawIdx* idxDst = (ImDrawIdx*)indexMappeds_[frameIdx];

    for (int i = 0; i < drawData->CmdListsCount; i++) {
        const ImDrawList* cmdList = drawData->CmdLists[i];
        memcpy(vtxDst, cmdList->VtxBuffer.Data, cmdList->VtxBuffer.Size * sizeof(ImDrawVert));
        memcpy(idxDst, cmdList->IdxBuffer.Data, cmdList->IdxBuffer.Size * sizeof(ImDrawIdx));
        vtxDst += cmdList->VtxBuffer.Size;
        idxDst += cmdList->IdxBuffer.Size;
    }

    std::array<VkMappedMemoryRange, 2> mappedMemoryRanges{};
    mappedMemoryRanges[0].sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
    mappedMemoryRanges[0].memory = vertexBufferMemorys_[frameIdx];
    mappedMemoryRanges[0].offset = 0;
    mappedMemoryRanges[0].size = vertexAllocationSizes_[frameIdx];
    mappedMemoryRanges[1].sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
    mappedMemoryRanges[1].memory = indexBufferMemorys_[frameIdx];
    mappedMemoryRanges[1].offset = 0;
    mappedMemoryRanges[1].size = indexAllocationSizes_[frameIdx];

    VK_CHECK(vkFlushMappedMemoryRanges(device_->get(),
                                       static_cast<uint32_t>(mappedMemoryRanges.size()),
                                       mappedMemoryRanges.data()));
}

void RendererGui::draw(VkCommandBuffer cmd, uint32_t frameIdx,
                       const std::shared_ptr<Image2D> renderTarget)
{
    VkRenderingAttachmentInfo renderingAttachmentInfo{};
    renderingAttachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    renderingAttachmentInfo.imageView = renderTarget->view();
    renderingAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    renderingAttachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    renderingAttachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    VkRenderingInfo renderingInfo{VK_STRUCTURE_TYPE_RENDERING_INFO};
    renderingInfo.renderArea = {0, 0, renderTarget->width(), renderTarget->height()};
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments = &renderingAttachmentInfo;

    ImDrawData* drawData = ImGui::GetDrawData();
    if (!drawData || !drawData->CmdListsCount) {
        return;
    }

    vkCmdBeginRendering(cmd, &renderingInfo);

    VkViewport viewport{};
    viewport.x = 0.f;
    viewport.y = 0.f;
    viewport.width = static_cast<float>(renderTarget->width());
    viewport.height = static_cast<float>(renderTarget->height());
    viewport.minDepth = 0.f;
    viewport.maxDepth = 1.f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout_, 0, 1,
                            &descriptorSet_, 0, NULL);

    ImGuiIO& io = ImGui::GetIO();
    GuiPushConstants pc{};
    pc.scale = glm::vec2(2.0f / io.DisplaySize.x, 2.0f / io.DisplaySize.y);
    pc.translate = glm::vec2(-1.0f);
    vkCmdPushConstants(cmd, pipelineLayout_, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(GuiPushConstants),
                       &pc);

    VkDeviceSize offsets[1] = {0};
    vkCmdBindVertexBuffers(cmd, 0, 1, &vertexBuffers_[frameIdx], offsets);
    vkCmdBindIndexBuffer(cmd, indexBuffers_[frameIdx], 0, VK_INDEX_TYPE_UINT16);

    int32_t vertexOffset = 0;
    int32_t indexOffset = 0;

    for (int32_t i = 0; i < drawData->CmdListsCount; i++) {
        const ImDrawList* cmdList = drawData->CmdLists[i];
        for (int32_t j = 0; j < cmdList->CmdBuffer.Size; j++) {
            const ImDrawCmd* pcmd = &cmdList->CmdBuffer[j];
            VkRect2D scissorRect{};
            scissorRect.offset.x = std::max((int32_t)(pcmd->ClipRect.x), 0);
            scissorRect.offset.y = std::max((int32_t)(pcmd->ClipRect.y), 0);
            scissorRect.extent.width = (uint32_t)(pcmd->ClipRect.z - pcmd->ClipRect.x);
            scissorRect.extent.height = (uint32_t)(pcmd->ClipRect.w - pcmd->ClipRect.y);
            vkCmdSetScissor(cmd, 0, 1, &scissorRect);
            vkCmdDrawIndexed(cmd, pcmd->ElemCount, 1, indexOffset, vertexOffset, 0);
            indexOffset += pcmd->ElemCount;
        }

        vertexOffset += cmdList->VtxBuffer.Size;
    }

    vkCmdEndRendering(cmd);
}

void RendererGui::init()
{
    ImGui::CreateContext();
    ImGuiStyle& style = ImGui::GetStyle();
    style.Colors[ImGuiCol_TitleBg] = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
    style.Colors[ImGuiCol_TitleBgActive] = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
    style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(1.0f, 0.0f, 0.0f, 0.1f);
    style.Colors[ImGuiCol_MenuBarBg] = ImVec4(1.0f, 0.0f, 0.0f, 0.4f);
    style.Colors[ImGuiCol_Header] = ImVec4(0.8f, 0.0f, 0.0f, 0.4f);
    style.Colors[ImGuiCol_HeaderActive] = ImVec4(1.0f, 0.0f, 0.0f, 0.4f);
    style.Colors[ImGuiCol_HeaderHovered] = ImVec4(1.0f, 0.0f, 0.0f, 0.4f);
    style.Colors[ImGuiCol_FrameBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.8f);
    style.Colors[ImGuiCol_CheckMark] = ImVec4(1.0f, 0.0f, 0.0f, 0.8f);
    style.Colors[ImGuiCol_SliderGrab] = ImVec4(1.0f, 0.0f, 0.0f, 0.4f);
    style.Colors[ImGuiCol_SliderGrabActive] = ImVec4(1.0f, 0.0f, 0.0f, 0.8f);
    style.Colors[ImGuiCol_FrameBgHovered] = ImVec4(1.0f, 1.0f, 1.0f, 0.1f);
    style.Colors[ImGuiCol_FrameBgActive] = ImVec4(1.0f, 1.0f, 1.0f, 0.2f);
    style.Colors[ImGuiCol_Button] = ImVec4(1.0f, 0.0f, 0.0f, 0.4f);
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(1.0f, 0.0f, 0.0f, 0.6f);
    style.Colors[ImGuiCol_ButtonActive] = ImVec4(1.0f, 0.0f, 0.0f, 0.8f);
    style.ScaleAllSizes(scale_);

    ImGuiIO& io = ImGui::GetIO();
    io.FontGlobalScale = scale_;

    const std::string font = "assets\\Noto_Sans_KR\\static\\NotoSansKR-SemiBold.ttf";

    ImFontConfig config{};
    config.MergeMode = false;
    io.Fonts->AddFontFromFileTTF(font.c_str(), 16.0f * scale_, &config,
                                 io.Fonts->GetGlyphRangesDefault());
    config.MergeMode = true;
    io.Fonts->AddFontFromFileTTF(font.c_str(), 16.0f * scale_, &config,
                                 io.Fonts->GetGlyphRangesKorean());

    unsigned char* data{};
    int width{}, height{};
    io.Fonts->GetTexDataAsRGBA32(&data, &width, &height);

    if (!data) {
        exitLog("Failed to load font data from: {}", font);
    }

    fontTexture_->createTexture(data, width, height, 4, false);
    fontTexture_->setSampler(device_->samplerAnisoRepeat());
}

void RendererGui::createDescriptorSetLayout()
{
    VkDescriptorSetLayoutBinding descriptorSetLayoutBinding{};
    descriptorSetLayoutBinding.binding = 0;
    descriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorSetLayoutBinding.descriptorCount = 1;
    descriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCI{};
    descriptorSetLayoutCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptorSetLayoutCI.bindingCount = 1;
    descriptorSetLayoutCI.pBindings = &descriptorSetLayoutBinding;

    VK_CHECK(vkCreateDescriptorSetLayout(device_->get(), &descriptorSetLayoutCI, nullptr,
                                         &descriptorSetLayout_));
}

void RendererGui::allocateDescriptorSets()
{
    VkDescriptorSetAllocateInfo descriptorSetAllocateInfo{};
    descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descriptorSetAllocateInfo.descriptorPool = device_->descriptorPool();
    descriptorSetAllocateInfo.descriptorSetCount = 1;
    descriptorSetAllocateInfo.pSetLayouts = &descriptorSetLayout_;

    VK_CHECK(vkAllocateDescriptorSets(device_->get(), &descriptorSetAllocateInfo, &descriptorSet_));

    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView = fontTexture_->view();
    imageInfo.sampler = fontTexture_->sampler();

    VkWriteDescriptorSet descriptorWrite{};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet = descriptorSet_;
    descriptorWrite.dstBinding = 0;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pImageInfo = &imageInfo;

    vkUpdateDescriptorSets(device_->get(), 1, &descriptorWrite, 0, nullptr);
}

void RendererGui::createPipelineLayout()
{
    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(GuiPushConstants);

    VkPipelineLayoutCreateInfo pipelineLayoutCI{};
    pipelineLayoutCI.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutCI.setLayoutCount = 1;
    pipelineLayoutCI.pSetLayouts = &descriptorSetLayout_;
    pipelineLayoutCI.pushConstantRangeCount = 1;
    pipelineLayoutCI.pPushConstantRanges = &pushConstantRange;

    VK_CHECK(vkCreatePipelineLayout(device_->get(), &pipelineLayoutCI, nullptr, &pipelineLayout_));
}

void RendererGui::createPipeline(VkFormat colorFormat)
{
    VkShaderModule vertexModule = device_->createShaderModule("shaders/imgui.vert.spv");
    VkShaderModule fragmentModule = device_->createShaderModule("shaders/imgui.frag.spv");

    std::array<VkPipelineShaderStageCreateInfo, 2> shaderSCIs{};
    shaderSCIs[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderSCIs[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    shaderSCIs[0].module = vertexModule;
    shaderSCIs[0].pName = "main";

    shaderSCIs[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderSCIs[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shaderSCIs[1].module = fragmentModule;
    shaderSCIs[1].pName = "main";

    VkVertexInputBindingDescription inputBinding{};
    inputBinding.binding = 0;
    inputBinding.stride = sizeof(ImDrawVert);
    inputBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    std::vector<VkVertexInputAttributeDescription> inputAttributes(3);
    inputAttributes[0].binding = 0;
    inputAttributes[0].location = 0;
    inputAttributes[0].format = VK_FORMAT_R32G32_SFLOAT;
    inputAttributes[0].offset = offsetof(ImDrawVert, pos);

    inputAttributes[1].binding = 0;
    inputAttributes[1].location = 1;
    inputAttributes[1].format = VK_FORMAT_R32G32_SFLOAT;
    inputAttributes[1].offset = offsetof(ImDrawVert, uv);

    inputAttributes[2].binding = 0;
    inputAttributes[2].location = 2;
    inputAttributes[2].format = VK_FORMAT_R8G8B8A8_UNORM;
    inputAttributes[2].offset = offsetof(ImDrawVert, col);

    VkPipelineVertexInputStateCreateInfo vertexInputSCI{};
    vertexInputSCI.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputSCI.vertexBindingDescriptionCount = 1;
    vertexInputSCI.pVertexBindingDescriptions = &inputBinding;
    vertexInputSCI.vertexAttributeDescriptionCount = static_cast<uint32_t>(inputAttributes.size());
    vertexInputSCI.pVertexAttributeDescriptions = inputAttributes.data();

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
    rasterizationSCI.lineWidth = 1.f;
    rasterizationSCI.cullMode = VK_CULL_MODE_NONE;
    rasterizationSCI.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizationSCI.depthBiasEnable = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisampleSCI{};
    multisampleSCI.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampleSCI.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampleSCI.sampleShadingEnable = VK_FALSE;

    VkPipelineDepthStencilStateCreateInfo depthStencilSCI{};
    depthStencilSCI.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencilSCI.depthTestEnable = VK_FALSE;
    depthStencilSCI.depthWriteEnable = VK_FALSE;
    depthStencilSCI.depthCompareOp = VK_COMPARE_OP_ALWAYS;
    depthStencilSCI.depthBoundsTestEnable = VK_FALSE;
    depthStencilSCI.minDepthBounds = 0.f;
    depthStencilSCI.maxDepthBounds = 1.f;
    depthStencilSCI.stencilTestEnable = VK_FALSE;

    VkPipelineColorBlendAttachmentState colorBlendAttachmentState{};
    colorBlendAttachmentState.blendEnable = VK_TRUE;
    colorBlendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    colorBlendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorBlendAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
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
    renderingCI.depthAttachmentFormat = VK_FORMAT_UNDEFINED;
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

void RendererGui::createBuffer(uint32_t frameIdx, VkBufferUsageFlagBits usage, VkDeviceSize size)
{
    VkBuffer& buffer = usage & VK_BUFFER_USAGE_VERTEX_BUFFER_BIT ? vertexBuffers_[frameIdx]
                                                                 : indexBuffers_[frameIdx];
    VkDeviceMemory& memory = usage & VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
                                 ? vertexBufferMemorys_[frameIdx]
                                 : indexBufferMemorys_[frameIdx];
    void*& mapped = usage & VK_BUFFER_USAGE_VERTEX_BUFFER_BIT ? vertexMappeds_[frameIdx]
                                                              : indexMappeds_[frameIdx];
    VkDeviceSize& allocationSize = usage & VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
                                       ? vertexAllocationSizes_[frameIdx]
                                       : indexAllocationSizes_[frameIdx];

    if (mapped) {
        vkUnmapMemory(device_->get(), memory);
        mapped = nullptr;
    }
    if (buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(device_->get(), buffer, nullptr);
        buffer = VK_NULL_HANDLE;
    }
    if (memory != VK_NULL_HANDLE) {
        vkFreeMemory(device_->get(), memory, nullptr);
        memory = VK_NULL_HANDLE;
    }

    VkBufferCreateInfo bufferCreateInfo{};
    bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferCreateInfo.size = size;
    bufferCreateInfo.usage = usage;
    bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VK_CHECK(vkCreateBuffer(device_->get(), &bufferCreateInfo, nullptr, &buffer));

    VkMemoryRequirements memoryRequirements;
    vkGetBufferMemoryRequirements(device_->get(), buffer, &memoryRequirements);
    allocationSize = memoryRequirements.size;

    VkMemoryAllocateInfo memoryAllocateInfo{};
    memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memoryAllocateInfo.allocationSize = allocationSize;
    memoryAllocateInfo.memoryTypeIndex = device_->getMemoryTypeIndex(
        memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

    VK_CHECK(vkAllocateMemory(device_->get(), &memoryAllocateInfo, nullptr, &memory));
    VK_CHECK(vkBindBufferMemory(device_->get(), buffer, memory, 0));

    VK_CHECK(vkMapMemory(device_->get(), memory, 0, allocationSize, 0, &mapped));
}

} // namespace guk