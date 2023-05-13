/* a single glTF node */
#pragma once
#include <vector>
#include <memory>
#include <string>
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>

class GltfNode : public std::enable_shared_from_this<GltfNode> {
  public:
    static std::shared_ptr<GltfNode> createRoot(int rootNodeNum);
    void addChilds(std::vector<int> childNodes);
    std::vector<std::shared_ptr<GltfNode>> getChilds();
    int getNodeNum();

    void setNodeName(std::string name);
    void setScale(glm::vec3 scale);
    void setTranslation(glm::vec3 translation);
    void setRotation(glm::quat rotation);
    void setMatrix(glm::mat4 matrix);

    void calculateLocalTRSMatrix();
    glm::mat4 getNodeMatrix();

    void printTree();

  private:
    void printNodes(std::shared_ptr<GltfNode> startNode, int indent);

    int mNodeNum = 0;
    std::string mNodeName;

    /* std::weak_ptr to break circular depenency between parent and child nodes */
    std::weak_ptr<GltfNode> mParentNode;
    std::vector<std::shared_ptr<GltfNode>> mChildNodes{};

    glm::vec3 mScale = glm::vec3(1.0f);
    glm::vec3 mTranslation = glm::vec3(0.0f);
    glm::quat mRotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    glm::mat4 mMatrix = glm::mat4(1.0f);

    glm::mat4 mLocalTRSMatrix = glm::mat4(1.0f);
    glm::mat4 mNodeMatrix = glm::mat4(1.0f);
    glm::mat4 mInverseBindMatrix = glm::mat4(1.0f);
};
