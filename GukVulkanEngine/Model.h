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

    void load(const std::string& model);
    const std::vector<Mesh>& meshes() const;

    void allocateDescriptorSets(VkDescriptorSetLayout layout,
                                std::shared_ptr<Image2D> dummyTexture);
    VkDescriptorSet getDescriptorSets(uint32_t index) const;

  private:
    std::shared_ptr<Device> device_;

    std::vector<Mesh> meshes_{};

    std::vector<MaterialUniform> materials_;
    std::vector<std::shared_ptr<Buffer>> materialUniformBuffers_;

    std::vector<std::shared_ptr<Image2D>> textures_;
    std::vector<std::string> textureFiles_;
    std::vector<bool> textureSrgb_;

    std::vector<VkDescriptorSet> descriptorSets_{};

    void processNode(aiNode* node, const aiScene* scene, glm::mat4 matrix);
    Mesh processMesh(aiMesh* mesh, const aiScene* scene, glm::mat4 matrix);

    void processMaterial(const aiScene* scene);
    uint32_t getTextureIndex(const std::string& textureFile, bool srgb);

    void createBuffers();
    void createTextures(const aiScene* scene);


};

} // namespace guk