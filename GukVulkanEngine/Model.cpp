#include "Model.h"
#include "Logger.h"

#include <assimp\Importer.hpp>
#include <assimp\postprocess.h>
#include <glm/gtc/type_ptr.hpp>

namespace guk {

Model::Model(std::shared_ptr<Device> device) : device_(device)
{
}

void Model::load(const std::string& model)
{
    Assimp::Importer aiImporter;
    const aiScene* scene = aiImporter.ReadFile(model, aiProcess_Triangulate);

    processMesh(scene->mRootNode, scene, glm::mat4{1.f});
    processMaterial(scene);
    createTextures(scene);
}

const std::vector<Mesh>& Model::meshes() const
{
    return meshes_;
}

glm::mat4 Model::matrix() const
{
    return matrix_;
}

void Model::transform(const glm::mat4& matrix)
{
    matrix_ = matrix * matrix_;
}

VkDescriptorSet Model::getDescriptorSets(uint32_t index) const
{
    return descriptorSets_[index];
}

void Model::allocateDescriptorSets(VkDescriptorSetLayout layout,
                                   std::shared_ptr<Image2D> dummyTexture)
{
    descriptorSets_.resize(materials_.size());

    std::vector<VkDescriptorSetLayout> layouts(materials_.size(), layout);

    VkDescriptorSetAllocateInfo descSetAI{};
    descSetAI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descSetAI.descriptorPool = device_->descriptorPool();
    descSetAI.descriptorSetCount = static_cast<uint32_t>(layouts.size());
    descSetAI.pSetLayouts = layouts.data();

    VK_CHECK(vkAllocateDescriptorSets(device_->get(), &descSetAI, descriptorSets_.data()));

    for (size_t i = 0; i < materials_.size(); i++) {
        const MaterialUniform& m = materials_[i];

        VkDescriptorBufferInfo uniformInfo{};
        uniformInfo.buffer = materialUniformBuffers_[i]->get();
        uniformInfo.range = sizeof(MaterialUniform);

        VkDescriptorImageInfo baseColorInfo{};
        baseColorInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        baseColorInfo.imageView = m.baseColorTextureIndex_ < 0
                                      ? dummyTexture->view()
                                      : textures_[m.baseColorTextureIndex_]->view();
        baseColorInfo.sampler = m.baseColorTextureIndex_ < 0
                                    ? dummyTexture->sampler()
                                    : textures_[m.baseColorTextureIndex_]->sampler();

        VkDescriptorImageInfo emissiveInfo{};
        emissiveInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        emissiveInfo.imageView = m.emissiveTextureIndex_ < 0
                                     ? dummyTexture->view()
                                     : textures_[m.emissiveTextureIndex_]->view();
        emissiveInfo.sampler = m.emissiveTextureIndex_ < 0
                                   ? dummyTexture->sampler()
                                   : textures_[m.emissiveTextureIndex_]->sampler();

        VkDescriptorImageInfo normalInfo{};
        normalInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        normalInfo.imageView = m.normalTextureIndex_ < 0 ? dummyTexture->view()
                                                         : textures_[m.normalTextureIndex_]->view();
        normalInfo.sampler = m.normalTextureIndex_ < 0
                                 ? dummyTexture->sampler()
                                 : textures_[m.normalTextureIndex_]->sampler();

        VkDescriptorImageInfo metallicRoughnessInfo{};
        metallicRoughnessInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        metallicRoughnessInfo.imageView = m.metallicRoughnessTextureIndex_ < 0
                                              ? dummyTexture->view()
                                              : textures_[m.metallicRoughnessTextureIndex_]->view();
        metallicRoughnessInfo.sampler =
            m.metallicRoughnessTextureIndex_ < 0
                ? dummyTexture->sampler()
                : textures_[m.metallicRoughnessTextureIndex_]->sampler();

        VkDescriptorImageInfo occlusionInfo{};
        occlusionInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        occlusionInfo.imageView = m.occlusionTextureIndex_ < 0
                                      ? dummyTexture->view()
                                      : textures_[m.occlusionTextureIndex_]->view();
        occlusionInfo.sampler = m.occlusionTextureIndex_ < 0
                                    ? dummyTexture->sampler()
                                    : textures_[m.occlusionTextureIndex_]->sampler();

        std::array<VkWriteDescriptorSet, 6> write{};
        write[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write[0].dstSet = descriptorSets_[i];
        write[0].dstBinding = 0;
        write[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        write[0].descriptorCount = 1;
        write[0].pBufferInfo = &uniformInfo;

        write[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write[1].dstSet = descriptorSets_[i];
        write[1].dstBinding = 1;
        write[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        write[1].descriptorCount = 1;
        write[1].pImageInfo = &baseColorInfo;

        write[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write[2].dstSet = descriptorSets_[i];
        write[2].dstBinding = 2;
        write[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        write[2].descriptorCount = 1;
        write[2].pImageInfo = &emissiveInfo;

        write[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write[3].dstSet = descriptorSets_[i];
        write[3].dstBinding = 3;
        write[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        write[3].descriptorCount = 1;
        write[3].pImageInfo = &normalInfo;

        write[4].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write[4].dstSet = descriptorSets_[i];
        write[4].dstBinding = 4;
        write[4].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        write[4].descriptorCount = 1;
        write[4].pImageInfo = &metallicRoughnessInfo;

        write[5].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write[5].dstSet = descriptorSets_[i];
        write[5].dstBinding = 5;
        write[5].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        write[5].descriptorCount = 1;
        write[5].pImageInfo = &occlusionInfo;

        vkUpdateDescriptorSets(device_->get(), static_cast<uint32_t>(write.size()), write.data(), 0,
                               nullptr);
    }
}

void Model::processMesh(aiNode* node, const aiScene* scene, glm::mat4 matrix)
{
    matrix *= glm::transpose(glm::make_mat4(&node->mTransformation.a1));

    for (uint32_t i = 0; i < node->mNumMeshes; i++) {
        aiMesh* aiMesh = scene->mMeshes[node->mMeshes[i]];
        Mesh mesh{device_};

        for (uint32_t i = 0; i < aiMesh->mNumVertices; i++) {
            Vertex vertex{};

            vertex.position.x = aiMesh->mVertices[i].x;
            vertex.position.y = aiMesh->mVertices[i].y;
            vertex.position.z = aiMesh->mVertices[i].z;
            vertex.position = glm::vec3(glm::vec4(vertex.position, 1.f) * matrix);

            vertex.normal.x = aiMesh->mNormals[i].x;
            vertex.normal.y = aiMesh->mNormals[i].z;
            vertex.normal.z = -aiMesh->mNormals[i].y;

            vertex.texcoord.x = (float)aiMesh->mTextureCoords[0][i].x;
            vertex.texcoord.y = 1.0f - (float)aiMesh->mTextureCoords[0][i].y;

            mesh.addVertex(vertex);
        }

        for (uint32_t i = 0; i < aiMesh->mNumFaces; i++) {
            aiFace face = aiMesh->mFaces[i];
            for (uint32_t j = 0; j < face.mNumIndices; j++) {

                mesh.addIndex(face.mIndices[j]);
            }
        }

        mesh.calculateTangents();
        mesh.setMaterialIndex(aiMesh->mMaterialIndex);
        mesh.createVertexBuffer();
        mesh.createIndexBuffer();

        meshes_.push_back(mesh);
    }

    for (uint32_t i = 0; i < node->mNumChildren; i++) {
        processMesh(node->mChildren[i], scene, matrix);
    }
}

void Model::processMaterial(const aiScene* scene)
{
    for (uint32_t i = 0; i < scene->mNumMaterials; i++) {
        aiMaterial* aiMaterial = scene->mMaterials[i];
        MaterialUniform material{};

        aiColor3D color;
        if (aiMaterial->Get(AI_MATKEY_COLOR_DIFFUSE, color) == AI_SUCCESS) {
            material.baseColorFactor_ = glm::vec4(color.r, color.g, color.b, 1.f);
        }

        float metailic;
        if (aiMaterial->Get(AI_MATKEY_METALLIC_FACTOR, metailic) == AI_SUCCESS) {
            material.metallicFactor_ = metailic;
        }

        float roughness;
        if (aiMaterial->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughness) == AI_SUCCESS) {
            material.roughness_ = roughness;
        }

        aiColor3D emissive;
        if (aiMaterial->Get(AI_MATKEY_COLOR_EMISSIVE, emissive) == AI_SUCCESS) {
            material.emissiveFactor_ = glm::vec4(emissive.r, emissive.g, emissive.b, 1.f);
        }

        aiString path;

        if (aiMaterial->GetTexture(aiTextureType_DIFFUSE, 0, &path) == AI_SUCCESS) {
            material.baseColorTextureIndex_ = getTextureIndex(path.C_Str(), true);
        }

        if (aiMaterial->GetTexture(aiTextureType_GLTF_METALLIC_ROUGHNESS, 0, &path) == AI_SUCCESS) {
            material.metallicRoughnessTextureIndex_ = getTextureIndex(path.C_Str(), false);
        }

        if (aiMaterial->GetTexture(aiTextureType_NORMALS, 0, &path) == AI_SUCCESS) {
            material.normalTextureIndex_ = getTextureIndex(path.C_Str(), false);
        }

        if (aiMaterial->GetTexture(aiTextureType_LIGHTMAP, 0, &path) == AI_SUCCESS) {
            material.occlusionTextureIndex_ = getTextureIndex(path.C_Str(), false);
        }

        if (aiMaterial->GetTexture(aiTextureType_EMISSIVE, 0, &path) == AI_SUCCESS) {
            material.emissiveTextureIndex_ = getTextureIndex(path.C_Str(), true);
        }

        materials_.push_back(material);

        std::shared_ptr<Buffer> uniformBuffer = std::make_shared<Buffer>(device_);
        uniformBuffer->createUniformBuffer(sizeof(MaterialUniform));
        uniformBuffer->update(material);

        materialUniformBuffers_.push_back(uniformBuffer);
    }
}

uint32_t Model::getTextureIndex(const std::string& textureFile, bool srgb)
{
    auto it = std::find(textureFiles_.begin(), textureFiles_.end(), textureFile);

    if (it != textureFiles_.end()) {
        return static_cast<uint32_t>(std::distance(textureFiles_.begin(), it));
    } else {
        textureFiles_.push_back(textureFile);
        textureSrgb_.push_back(srgb);
        return static_cast<uint32_t>(textureFiles_.size() - 1);
    }
}

void Model::createTextures(const aiScene* scene)
{
    for (uint32_t i = 0; i < textureFiles_.size(); i++) {
        std::shared_ptr<Image2D> texture = std::make_shared<Image2D>(device_);

        std::string file = textureFiles_[i];
        if (file[0] == '*') {
            int texIdx = std::stoi(file.substr(1));
            const aiTexture* aiTex = scene->mTextures[texIdx];

            if (aiTex->mHeight == 0) {
                texture->createTextureFromMemory(reinterpret_cast<unsigned char*>(aiTex->pcData),
                                                 aiTex->mWidth, textureSrgb_[i]);
            } else {
                int width = aiTex->mWidth;
                int height = aiTex->mHeight;
                int channels = 4;
                unsigned char* data = new unsigned char[width * height * channels];

                for (int i = 0; i < width * height; i++) {
                    data[i * channels + 0] = aiTex->pcData[i].r;
                    data[i * channels + 1] = aiTex->pcData[i].g;
                    data[i * channels + 2] = aiTex->pcData[i].b;
                    data[i * channels + 3] = aiTex->pcData[i].a;
                }

                texture->createTexture(data, width, height, channels, textureSrgb_[i]);

                delete[] data;
            }
        } else {
            texture->createTexture(file, textureSrgb_[i]);
        }

        texture->setSampler(device_->samplerLinearRepeat());
        textures_.push_back(texture);
    }
}

} // namespace guk
