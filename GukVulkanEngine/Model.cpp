#include "Model.h"
#include "Logger.h"

#include <filesystem>
#include <assimp\Importer.hpp>
#include <assimp\postprocess.h>
#include <glm/gtc/type_ptr.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

namespace guk {

Model::Model(std::shared_ptr<Device> device) : device_(device)
{
}

Model Model::load(std::shared_ptr<Device> device, const std::string& file)
{
    Model model{device};

    std::filesystem::path path(file);
    model.directory_ = path.parent_path().string() + "\\";
    model.name_ = path.stem().string();
    model.extension_ = path.extension().string();

    Assimp::Importer aiImporter;
    const aiScene* scene = aiImporter.ReadFile(file, aiProcess_Triangulate);

    model.processMesh(scene->mRootNode, scene, glm::mat4{1.f});
    model.normalizeModel();
    model.createMeshBuffers();

    model.processMaterial(scene);
    model.createTextures(scene);

    return model;
}

std::string Model::name() const
{
    return name_;
}

bool& Model::visible()
{
    return visible_;
}

const std::vector<Mesh>& Model::meshes() const
{
    return meshes_;
}

glm::mat4 Model::matrix() const
{
    glm::mat4 T = glm::translate(glm::mat4(1.0f), translation_);
    glm::mat4 R = glm::toMat4(glm::quat(glm::radians(rotation_)));
    glm::mat4 S = glm::scale(glm::mat4(1.0f), scale_);

    return T * R * S;
}

glm::vec3 Model::getTranslation() const
{
    return translation_;
}

Model Model::setTranslation(glm::vec3 translation)
{
    translation_ = translation;
    return *this;
}

glm::vec3 Model::getRotation() const
{
    return rotation_;
}

Model Model::setRotation(glm::vec3 rotation)
{
    rotation_ = rotation;
    return *this;
}

glm::vec3 Model::getScale() const
{
    return scale_;
}

Model Model::setScale(glm::vec3 scale)
{
    scale_ = scale;
    return *this;
}

VkDescriptorSet Model::getMaterialDescriptorSets(uint32_t index) const
{
    return materialDescriptorSets_[index];
}

void Model::allocateMaterialDescriptorSets(VkDescriptorSetLayout layout,
                                           std::shared_ptr<Image2D> dummyTexture)
{
    materialDescriptorSets_.resize(materials_.size());

    std::vector<VkDescriptorSetLayout> layouts(materials_.size(), layout);

    VkDescriptorSetAllocateInfo descSetAI{};
    descSetAI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descSetAI.descriptorPool = device_->descriptorPool();
    descSetAI.descriptorSetCount = static_cast<uint32_t>(layouts.size());
    descSetAI.pSetLayouts = layouts.data();

    VK_CHECK(vkAllocateDescriptorSets(device_->get(), &descSetAI, materialDescriptorSets_.data()));

    for (size_t i = 0; i < materials_.size(); i++) {
        const MaterialUniform& m = materials_[i];

        VkDescriptorBufferInfo uniformInfo{};
        uniformInfo.buffer = materialUniformBuffers_[i]->get();
        uniformInfo.range = sizeof(MaterialUniform);

        VkDescriptorImageInfo baseColorInfo{};
        baseColorInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        baseColorInfo.imageView = m.baseColorTextureIndex < 0
                                      ? dummyTexture->view()
                                      : textures_[m.baseColorTextureIndex]->view();
        baseColorInfo.sampler = m.baseColorTextureIndex < 0
                                    ? dummyTexture->sampler()
                                    : textures_[m.baseColorTextureIndex]->sampler();

        VkDescriptorImageInfo emissiveInfo{};
        emissiveInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        emissiveInfo.imageView = m.emissiveTextureIndex < 0
                                     ? dummyTexture->view()
                                     : textures_[m.emissiveTextureIndex]->view();
        emissiveInfo.sampler = m.emissiveTextureIndex < 0
                                   ? dummyTexture->sampler()
                                   : textures_[m.emissiveTextureIndex]->sampler();

        VkDescriptorImageInfo normalInfo{};
        normalInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        normalInfo.imageView = m.normalTextureIndex < 0 ? dummyTexture->view()
                                                         : textures_[m.normalTextureIndex]->view();
        normalInfo.sampler = m.normalTextureIndex < 0
                                 ? dummyTexture->sampler()
                                 : textures_[m.normalTextureIndex]->sampler();

        VkDescriptorImageInfo metallicRoughnessInfo{};
        metallicRoughnessInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        metallicRoughnessInfo.imageView = m.metallicRoughnessTextureIndex < 0
                                              ? dummyTexture->view()
                                              : textures_[m.metallicRoughnessTextureIndex]->view();
        metallicRoughnessInfo.sampler =
            m.metallicRoughnessTextureIndex < 0
                ? dummyTexture->sampler()
                : textures_[m.metallicRoughnessTextureIndex]->sampler();

        VkDescriptorImageInfo occlusionInfo{};
        occlusionInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        occlusionInfo.imageView = m.occlusionTextureIndex < 0
                                      ? dummyTexture->view()
                                      : textures_[m.occlusionTextureIndex]->view();
        occlusionInfo.sampler = m.occlusionTextureIndex < 0
                                    ? dummyTexture->sampler()
                                    : textures_[m.occlusionTextureIndex]->sampler();

        std::array<VkWriteDescriptorSet, 6> write{};
        write[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write[0].dstSet = materialDescriptorSets_[i];
        write[0].dstBinding = 0;
        write[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        write[0].descriptorCount = 1;
        write[0].pBufferInfo = &uniformInfo;

        write[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write[1].dstSet = materialDescriptorSets_[i];
        write[1].dstBinding = 1;
        write[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        write[1].descriptorCount = 1;
        write[1].pImageInfo = &baseColorInfo;

        write[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write[2].dstSet = materialDescriptorSets_[i];
        write[2].dstBinding = 2;
        write[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        write[2].descriptorCount = 1;
        write[2].pImageInfo = &emissiveInfo;

        write[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write[3].dstSet = materialDescriptorSets_[i];
        write[3].dstBinding = 3;
        write[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        write[3].descriptorCount = 1;
        write[3].pImageInfo = &normalInfo;

        write[4].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write[4].dstSet = materialDescriptorSets_[i];
        write[4].dstBinding = 4;
        write[4].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        write[4].descriptorCount = 1;
        write[4].pImageInfo = &metallicRoughnessInfo;

        write[5].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write[5].dstSet = materialDescriptorSets_[i];
        write[5].dstBinding = 5;
        write[5].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        write[5].descriptorCount = 1;
        write[5].pImageInfo = &occlusionInfo;

        vkUpdateDescriptorSets(device_->get(), static_cast<uint32_t>(write.size()), write.data(), 0,
                               nullptr);
    }
}

glm::vec3 Model::boundMin() const
{
    return boundMin_;
}

glm::vec3 Model::boundMax() const
{
    return boundMax_;
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
            if (extension_ == ".glb") {
                vertex.normal.y = aiMesh->mNormals[i].z;
                vertex.normal.z = -aiMesh->mNormals[i].y;
            } else {
                vertex.normal.y = aiMesh->mNormals[i].y;
                vertex.normal.z = aiMesh->mNormals[i].z;
            }

            if (aiMesh->mTextureCoords[0]) {
                vertex.texcoord.x = (float)aiMesh->mTextureCoords[0][i].x;
                vertex.texcoord.y = 1.0f - (float)aiMesh->mTextureCoords[0][i].y;
            }

            mesh.addVertex(vertex);
        }

        for (uint32_t i = 0; i < aiMesh->mNumFaces; i++) {
            aiFace face = aiMesh->mFaces[i];
            for (uint32_t j = 0; j < face.mNumIndices; j++) {

                mesh.addIndex(face.mIndices[j]);
            }
        }

        mesh.calculateTangents();
        mesh.calculateBound();
        mesh.setMaterialIndex(aiMesh->mMaterialIndex);

        meshes_.push_back(mesh);
    }

    for (uint32_t i = 0; i < node->mNumChildren; i++) {
        processMesh(node->mChildren[i], scene, matrix);
    }
}

void Model::normalizeModel()
{
    boundMin_ = glm::vec3(std::numeric_limits<float>::max());
    boundMax_ = glm::vec3(std::numeric_limits<float>::lowest());

    for (const auto& mesh : meshes_) {
        boundMin_ = glm::min(boundMin_, mesh.boundMin());
        boundMax_ = glm::max(boundMax_, mesh.boundMax());
    }

    glm::vec3 center = (boundMax_ + boundMin_) * 0.5f;
    float delta = glm::compMax(boundMax_ - boundMin_);

    for (auto& mesh : meshes_) {
        for (auto& vertex : mesh.vertices()) {
            vertex.position = (vertex.position - center) / delta;
        }
    }

    boundMin_ = (boundMin_ - center) / delta;
    boundMax_ = (boundMax_ - center) / delta;
}

void Model::createMeshBuffers()
{
    for (auto& mesh : meshes_) {
        mesh.createVertexBuffer();
        mesh.createIndexBuffer();
    }
}

void Model::processMaterial(const aiScene* scene)
{
    for (uint32_t i = 0; i < scene->mNumMaterials; i++) {
        aiMaterial* aiMaterial = scene->mMaterials[i];
        MaterialUniform material{};

        aiColor3D color;
        if (aiMaterial->Get(AI_MATKEY_COLOR_DIFFUSE, color) == AI_SUCCESS) {
            material.baseColorFactor = glm::vec4(color.r, color.g, color.b, 1.f);
        }

        float metailic;
        if (aiMaterial->Get(AI_MATKEY_METALLIC_FACTOR, metailic) == AI_SUCCESS) {
            material.metallicFactor = metailic;
        }

        float roughness;
        if (aiMaterial->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughness) == AI_SUCCESS) {
            material.roughness = roughness;
        }

        aiColor3D emissive;
        if (aiMaterial->Get(AI_MATKEY_COLOR_EMISSIVE, emissive) == AI_SUCCESS) {
            material.emissiveFactor = glm::vec4(emissive.r, emissive.g, emissive.b, 1.f);
        }

        aiString path;

        if (aiMaterial->GetTexture(aiTextureType_DIFFUSE, 0, &path) == AI_SUCCESS) {
            material.baseColorTextureIndex = getTextureIndex(path.C_Str(), true);
        }

        if (aiMaterial->GetTexture(aiTextureType_GLTF_METALLIC_ROUGHNESS, 0, &path) == AI_SUCCESS) {
            material.metallicRoughnessTextureIndex = getTextureIndex(path.C_Str(), false);
        }

        if (aiMaterial->GetTexture(aiTextureType_NORMALS, 0, &path) == AI_SUCCESS) {
            material.normalTextureIndex = getTextureIndex(path.C_Str(), false);
        }

        if (aiMaterial->GetTexture(aiTextureType_LIGHTMAP, 0, &path) == AI_SUCCESS) {
            material.occlusionTextureIndex = getTextureIndex(path.C_Str(), false);
        }

        if (aiMaterial->GetTexture(aiTextureType_EMISSIVE, 0, &path) == AI_SUCCESS) {
            material.emissiveTextureIndex = getTextureIndex(path.C_Str(), true);
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
            texture->createTexture(directory_ + file, textureSrgb_[i]);
        }

        texture->setSampler(device_->samplerLinearRepeat());
        textures_.push_back(texture);
    }
}

} // namespace guk
