#include "GuiRenderer.h"
#include "Logger.h"

#include <imgui.h>
#include <array>

namespace guk {

GuiRenderer::GuiRenderer(Engine& engine) : engine_(engine)
{
    init();
    createDescriptorSetLayout();
    allocateDescriptorSets();
    createPipelineLayout();
    createPipeline();
}

GuiRenderer::~GuiRenderer()
{
    vkDestroyPipeline(engine_.device_, pipeline_, nullptr);
    vkDestroyPipelineLayout(engine_.device_, pipelineLayout_, nullptr);
    vkDestroyDescriptorSetLayout(engine_.device_, descriptorSetLayout_, nullptr);

    vkDestroySampler(engine_.device_, textureSampler_, nullptr);
    vkDestroyImageView(engine_.device_, textureView_, nullptr);
    vkDestroyImage(engine_.device_, textureImage_, nullptr);
    vkFreeMemory(engine_.device_, textureMemory_, nullptr);

    for (size_t i = 0; i < engine_.MAX_FRAMES_IN_FLIGHT; i++) {
        vkUnmapMemory(engine_.device_, vertexBufferMemorys_[i]);
        vkDestroyBuffer(engine_.device_, vertexBuffers_[i], nullptr);
        vkFreeMemory(engine_.device_, vertexBufferMemorys_[i], nullptr);

        vkUnmapMemory(engine_.device_, indexBufferMemorys_[i]);
        vkDestroyBuffer(engine_.device_, indexBuffers_[i], nullptr);
        vkFreeMemory(engine_.device_, indexBufferMemorys_[i], nullptr);
    }
}

void GuiRenderer::update()
{
    ImGuiIO& io = ImGui::GetIO();

    // Update ImGui IO state
    io.DisplaySize = ImVec2(float(engine_.swapchainImageExtent_.width),
                            float(engine_.swapchainImageExtent_.height));
    io.MousePos = ImVec2(engine_.mouseState_.position.x, engine_.mouseState_.position.y);
    io.MouseDown[0] = engine_.mouseState_.buttons.left;
    io.MouseDown[1] = engine_.mouseState_.buttons.right;
    io.MouseDown[2] = engine_.mouseState_.buttons.middle;

    // Begin GUI frame
    ImGui::NewFrame();

    // Camera info window
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, 150), ImGuiCond_FirstUseEver);

    if (ImGui::Begin("Camera Control")) {
        ImGui::Separator();
        ImGui::Text("Controls:");
        ImGui::Text("Mouse: Look around");
        ImGui::Text("WASD: Move");
        ImGui::Text("QE: Up/Down");
        ImGui::Text("F2: Toggle camera mode");
    }
    ImGui::End();

    ImGui::Render();

    ImDrawData* drawData = ImGui::GetDrawData();
    if (!drawData || !drawData->TotalVtxCount || !drawData->TotalIdxCount) {
        return;
    }

    VkDeviceSize vertexBufferSize = drawData->TotalVtxCount * sizeof(ImDrawVert);
    VkDeviceSize indexBufferSize = drawData->TotalIdxCount * sizeof(ImDrawIdx);

    if (vertexBuffers_[engine_.currentFrame_] != VK_NULL_HANDLE ||
        vertexBufferSize > vertexAllocationSizes_[engine_.currentFrame_]) {

        VkDeviceSize newSize = std::max(static_cast<VkDeviceSize>(vertexBufferSize * 1.5f),
                                        static_cast<VkDeviceSize>(512 * sizeof(ImDrawVert)));
        createBuffer(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, newSize);
    }

    if (indexBuffers_[engine_.currentFrame_] != VK_NULL_HANDLE ||
        indexBufferSize > indexAllocationSizes_[engine_.currentFrame_]) {

        VkDeviceSize newSize = std::max(static_cast<VkDeviceSize>(vertexBufferSize * 1.5f),
                                        static_cast<VkDeviceSize>(1024 * sizeof(ImDrawIdx)));
        createBuffer(VK_BUFFER_USAGE_INDEX_BUFFER_BIT, newSize);
    }

    ImDrawVert* vtxDst = (ImDrawVert*)vertexMappeds_[engine_.currentFrame_];
    ImDrawIdx* idxDst = (ImDrawIdx*)indexMappeds_[engine_.currentFrame_];

    for (int i = 0; i < drawData->CmdListsCount; i++) {
        const ImDrawList* cmdList = drawData->CmdLists[i];
        memcpy(vtxDst, cmdList->VtxBuffer.Data, cmdList->VtxBuffer.Size * sizeof(ImDrawVert));
        memcpy(idxDst, cmdList->IdxBuffer.Data, cmdList->IdxBuffer.Size * sizeof(ImDrawIdx));
        vtxDst += cmdList->VtxBuffer.Size;
        idxDst += cmdList->IdxBuffer.Size;
    }

    std::array<VkMappedMemoryRange, 2> mappedMemoryRanges{};
    mappedMemoryRanges[0].sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
    mappedMemoryRanges[0].memory = vertexBufferMemorys_[engine_.currentFrame_];
    mappedMemoryRanges[0].offset = 0;
    mappedMemoryRanges[0].size = vertexAllocationSizes_[engine_.currentFrame_];
    mappedMemoryRanges[1].sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
    mappedMemoryRanges[1].memory = indexBufferMemorys_[engine_.currentFrame_];
    mappedMemoryRanges[1].offset = 0;
    mappedMemoryRanges[1].size = indexAllocationSizes_[engine_.currentFrame_];

    VK_CHECK(vkFlushMappedMemoryRanges(engine_.device_,
                                       static_cast<uint32_t>(mappedMemoryRanges.size()),
                                       mappedMemoryRanges.data()));
}

void GuiRenderer::draw()
{
    VkCommandBuffer commandBuffer{engine_.commandBuffers_[engine_.currentFrame_]};

    VkRenderingAttachmentInfo renderingAttachmentInfo{};
    renderingAttachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    renderingAttachmentInfo.imageView = engine_.swapChainImageViews_[engine_.currentImage_];
    renderingAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    renderingAttachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    renderingAttachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    VkRenderingInfo renderingInfo{VK_STRUCTURE_TYPE_RENDERING_INFO_KHR};
    renderingInfo.renderArea = {0, 0, engine_.swapchainImageExtent_.width,
                                engine_.swapchainImageExtent_.height};
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments = &renderingAttachmentInfo;

    ImDrawData* drawData = ImGui::GetDrawData();
    if (!drawData || !drawData->CmdListsCount) {
        return;
    }

    vkCmdBeginRendering(commandBuffer, &renderingInfo);

    VkViewport viewport{};
    viewport.x = 0.f;
    viewport.y = 0.f;
    viewport.width = static_cast<float>(engine_.swapchainImageExtent_.width);
    viewport.height = static_cast<float>(engine_.swapchainImageExtent_.height);
    viewport.minDepth = 0.f;
    viewport.maxDepth = 1.f;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout_, 0, 1,
                            &descriptorSet_, 0, NULL);

    ImGuiIO& io = ImGui::GetIO();
    PushConstants pc{};
    pc.scale = glm::vec2(2.0f / io.DisplaySize.x, 2.0f / io.DisplaySize.y);
    pc.translate = glm::vec2(-1.0f);
    vkCmdPushConstants(commandBuffer, pipelineLayout_, VK_SHADER_STAGE_VERTEX_BIT, 0,
                       sizeof(PushConstants), &pc);

    VkDeviceSize offsets[1] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffers_[engine_.currentFrame_], offsets);
    vkCmdBindIndexBuffer(commandBuffer, indexBuffers_[engine_.currentFrame_], 0,
                         VK_INDEX_TYPE_UINT16);

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
            vkCmdSetScissor(commandBuffer, 0, 1, &scissorRect);
            vkCmdDrawIndexed(commandBuffer, pcmd->ElemCount, 1, indexOffset, vertexOffset, 0);
            indexOffset += pcmd->ElemCount;
        }

        vertexOffset += cmdList->VtxBuffer.Size;
    }

    vkCmdEndRendering(commandBuffer);
}

void GuiRenderer::init()
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

    const std::string font = "./assets/Noto_Sans_KR/static/NotoSansKR-SemiBold.ttf";

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

    engine_.createTexture(textureImage_, textureMemory_, textureView_, textureSampler_, data, width,
                          height, 4, false, false);
}

void GuiRenderer::createDescriptorSetLayout()
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

    VK_CHECK(vkCreateDescriptorSetLayout(engine_.device_, &descriptorSetLayoutCI, nullptr,
                                         &descriptorSetLayout_));
}

void GuiRenderer::allocateDescriptorSets()
{
    VkDescriptorSetAllocateInfo descriptorSetAllocateInfo{};
    descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descriptorSetAllocateInfo.descriptorPool = engine_.descriptorPool_;
    descriptorSetAllocateInfo.descriptorSetCount = 1;
    descriptorSetAllocateInfo.pSetLayouts = &descriptorSetLayout_;

    VK_CHECK(
        vkAllocateDescriptorSets(engine_.device_, &descriptorSetAllocateInfo, &descriptorSet_));

    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView = textureView_;
    imageInfo.sampler = textureSampler_;

    VkWriteDescriptorSet descriptorWrite{};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet = descriptorSet_;
    descriptorWrite.dstBinding = 0;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pImageInfo = &imageInfo;

    vkUpdateDescriptorSets(engine_.device_, 1, &descriptorWrite, 0, nullptr);
}

void GuiRenderer::createPipelineLayout()
{
    VkPushConstantRange pushConstantRange{};
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(PushConstants);

    VkPipelineLayoutCreateInfo pipelineLayoutCI{};
    pipelineLayoutCI.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutCI.setLayoutCount = 1;
    pipelineLayoutCI.pSetLayouts = &descriptorSetLayout_;
    pipelineLayoutCI.pushConstantRangeCount = 1;
    pipelineLayoutCI.pPushConstantRanges = &pushConstantRange;

    VK_CHECK(vkCreatePipelineLayout(engine_.device_, &pipelineLayoutCI, nullptr, &pipelineLayout_));
}

void GuiRenderer::createPipeline()
{
    VkShaderModule vertexShaderModule = engine_.createShaderModule("shaders/imgui.vert.spv");
    VkShaderModule fragmentShaderModule = engine_.createShaderModule("shaders/imgui.frag.spv");

    VkPipelineShaderStageCreateInfo pipelineVertexShaderStageCI{};
    pipelineVertexShaderStageCI.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    pipelineVertexShaderStageCI.stage = VK_SHADER_STAGE_VERTEX_BIT;
    pipelineVertexShaderStageCI.module = vertexShaderModule;
    pipelineVertexShaderStageCI.pName = "main";

    VkPipelineShaderStageCreateInfo PipelineFragmentShaderStageCI{};
    PipelineFragmentShaderStageCI.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    PipelineFragmentShaderStageCI.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    PipelineFragmentShaderStageCI.module = fragmentShaderModule;
    PipelineFragmentShaderStageCI.pName = "main";

    VkPipelineShaderStageCreateInfo pipelineShaderStageCIs[] = {pipelineVertexShaderStageCI,
                                                                PipelineFragmentShaderStageCI};

    VkVertexInputBindingDescription vertexInputBindingDescrption{};
    vertexInputBindingDescrption.binding = 0;
    vertexInputBindingDescrption.stride = sizeof(ImDrawVert);
    vertexInputBindingDescrption.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    std::vector<VkVertexInputAttributeDescription> vertexInputAttributeDescriptions(3);
    vertexInputAttributeDescriptions[0].binding = 0;
    vertexInputAttributeDescriptions[0].location = 0;
    vertexInputAttributeDescriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
    vertexInputAttributeDescriptions[0].offset = offsetof(ImDrawVert, pos);

    vertexInputAttributeDescriptions[1].binding = 0;
    vertexInputAttributeDescriptions[1].location = 1;
    vertexInputAttributeDescriptions[1].format = VK_FORMAT_R32G32_SFLOAT;
    vertexInputAttributeDescriptions[1].offset = offsetof(ImDrawVert, uv);

    vertexInputAttributeDescriptions[2].binding = 0;
    vertexInputAttributeDescriptions[2].location = 2;
    vertexInputAttributeDescriptions[2].format = VK_FORMAT_R8G8B8A8_UNORM;
    vertexInputAttributeDescriptions[2].offset = offsetof(ImDrawVert, col);

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
    pipelineRasterizationStateCI.cullMode = VK_CULL_MODE_NONE;
    pipelineRasterizationStateCI.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    pipelineRasterizationStateCI.depthBiasEnable = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo pipelineMultisampleStateCI{};
    pipelineMultisampleStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    pipelineMultisampleStateCI.sampleShadingEnable = VK_FALSE;
    pipelineMultisampleStateCI.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo pipelineDepthStencilStateCI{};
    pipelineDepthStencilStateCI.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    pipelineDepthStencilStateCI.depthTestEnable = VK_FALSE;
    pipelineDepthStencilStateCI.depthWriteEnable = VK_FALSE;
    pipelineDepthStencilStateCI.depthCompareOp = VK_COMPARE_OP_ALWAYS;
    pipelineDepthStencilStateCI.depthBoundsTestEnable = VK_FALSE;
    pipelineDepthStencilStateCI.minDepthBounds = 0.f;
    pipelineDepthStencilStateCI.maxDepthBounds = 1.f;
    pipelineDepthStencilStateCI.stencilTestEnable = VK_FALSE;

    VkPipelineColorBlendAttachmentState pipelineColorBlendAttachmentState{};
    pipelineColorBlendAttachmentState.blendEnable = VK_TRUE;
    pipelineColorBlendAttachmentState.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
        VK_COLOR_COMPONENT_A_BIT;
    pipelineColorBlendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    pipelineColorBlendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    pipelineColorBlendAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;
    pipelineColorBlendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
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
    pipelineRenderingCI.pColorAttachmentFormats = &engine_.swapchainImageFormat_;
    pipelineRenderingCI.depthAttachmentFormat = VK_FORMAT_UNDEFINED;
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

void GuiRenderer::createBuffer(VkBufferUsageFlagBits usage, VkDeviceSize size)
{
    VkBuffer& buffer = usage & VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
                           ? vertexBuffers_[engine_.currentFrame_]
                           : indexBuffers_[engine_.currentFrame_];
    VkDeviceMemory& memory = usage & VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
                                 ? vertexBufferMemorys_[engine_.currentFrame_]
                                 : indexBufferMemorys_[engine_.currentFrame_];
    void*& mapped = usage & VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
                        ? vertexMappeds_[engine_.currentFrame_]
                        : indexMappeds_[engine_.currentFrame_];
    VkDeviceSize& allocationSize = usage & VK_BUFFER_USAGE_VERTEX_BUFFER_BIT
                                       ? vertexAllocationSizes_[engine_.currentFrame_]
                                       : indexAllocationSizes_[engine_.currentFrame_];

    if (mapped) {
        vkUnmapMemory(engine_.device_, memory);
        mapped = nullptr;
    }
    if (buffer != VK_NULL_HANDLE) {
        vkDestroyBuffer(engine_.device_, buffer, nullptr);
        buffer = VK_NULL_HANDLE;
    }
    if (memory != VK_NULL_HANDLE) {
        vkFreeMemory(engine_.device_, memory, nullptr);
        memory = VK_NULL_HANDLE;
    }

    VkBufferCreateInfo bufferCreateInfo{};
    bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferCreateInfo.size = size;
    bufferCreateInfo.usage = usage;
    bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VK_CHECK(vkCreateBuffer(engine_.device_, &bufferCreateInfo, nullptr, &buffer));

    VkMemoryRequirements memoryRequirements;
    vkGetBufferMemoryRequirements(engine_.device_, buffer, &memoryRequirements);
    allocationSize = memoryRequirements.size;

    VkMemoryAllocateInfo memoryAllocateInfo{};
    memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    memoryAllocateInfo.allocationSize = allocationSize;
    memoryAllocateInfo.memoryTypeIndex = engine_.getMemoryTypeIndex(
        memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);

    VK_CHECK(vkAllocateMemory(engine_.device_, &memoryAllocateInfo, nullptr, &memory));
    VK_CHECK(vkBindBufferMemory(engine_.device_, buffer, memory, 0));

    VK_CHECK(vkMapMemory(engine_.device_, memory, 0, allocationSize, 0, &mapped));
}

} // namespace guk