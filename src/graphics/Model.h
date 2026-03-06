#pragma once

#include "Mesh.h"

#include <string>
#include <vector>

// Forward declarations — keeps Assimp out of the header
struct aiNode;
struct aiMesh;
struct aiScene;

namespace engine {

/// Loads a 3D model file via Assimp and stores it as a collection of Meshes.
class Model
{
public:
    std::vector<Mesh> meshes;

    Model()  = default;
    ~Model() = default;

    Model(const Model&)            = delete;
    Model& operator=(const Model&) = delete;
    Model(Model&&)                 = default;
    Model& operator=(Model&&)      = default;

    /// Load a model from disk. Returns false on error.
    bool loadModel(const std::string& path);

    /// Draw every mesh in the model.
    void draw() const;

    /// Release all GPU resources.
    void shutdown();

private:
    /// Recursively walk the Assimp node tree.
    void processNode(aiNode* node, const aiScene* scene);

    /// Convert a single aiMesh into our engine::Mesh.
    Mesh processMesh(aiMesh* mesh, const aiScene* scene);
};

} // namespace engine
