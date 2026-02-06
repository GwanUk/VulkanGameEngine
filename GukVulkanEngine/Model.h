#pragma once

#include "Device.h"
#include "Mesh.h"
#include "Image2D.h"
#include "DataStructures.h"

#include <assimp\scene.h>
#include <glm/gtc/matrix_transform.hpp>

namespace guk {

class Model
{
  public:
    Model(std::shared_ptr<Device> device);

    static Model load(std::shared_ptr<Device> device, const std::string& file, bool normalizeModel = false);

    std::string name() const;
    bool& visible();
    const std::vector<Mesh>& meshes() const;
    glm::mat4 matrix() const;

    glm::vec3 getTranslation() const;
    Model setTranslation(glm::vec3 translation);
    glm::vec3 getRotation() const;
    Model setRotation(glm::vec3 rotation);
    glm::vec3 getScale() const;
    Model setScale(glm::vec3 scale);

    VkDescriptorSet getMaterialDescriptorSets(uint32_t index) const;
    void allocateMaterialDescriptorSets(VkDescriptorSetLayout layout,
                                        std::shared_ptr<Image2D> dummyTexture);

    glm::vec3 boundMin() const;
    glm::vec3 boundMax() const;

  private:
    std::shared_ptr<Device> device_;

    std::string name_{};
    std::string directory_{};
    std::string extension_{};
    bool visible_ = true;

    glm::vec3 translation_{};
    glm::vec3 rotation_{};
    glm::vec3 scale_{1.f};

    std::vector<Mesh> meshes_{};
    std::vector<MaterialUniform> materials_;
    std::vector<std::shared_ptr<Buffer>> materialUniformBuffers_;
    std::vector<VkDescriptorSet> materialDescriptorSets_{};

    std::vector<std::shared_ptr<Image2D>> textures_;
    std::vector<std::string> textureFiles_;
    std::vector<bool> textureSrgb_;

    glm::vec3 boundMin_{};
    glm::vec3 boundMax_{};

    void processMesh(aiNode* node, const aiScene* scene, glm::mat4 matrix);
    void createMeshBuffers();
    void calculateBound(bool normalizeModel);

    void processMaterial(const aiScene* scene);
    uint32_t getTextureIndex(const std::string& textureFile, bool srgb);
    void createTextures(const aiScene* scene);
};

} // namespace guk