#include "Model.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <cstdio>

namespace engine {

bool Model::loadModel(const std::string& path)
{
    Assimp::Importer importer;

    const aiScene* scene = importer.ReadFile(
        path,
        aiProcess_Triangulate | aiProcess_FlipUVs | aiProcess_GenNormals
    );

    if (!scene || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) || !scene->mRootNode)
    {
        std::fprintf(stderr, "[Model] Assimp error: %s\n", importer.GetErrorString());
        return false;
    }

    processNode(scene->mRootNode, scene);

    std::printf("[Model] Loaded '%s' — %zu mesh(es)\n", path.c_str(), meshes.size());
    return true;
}

void Model::draw() const
{
    for (const auto& mesh : meshes)
        mesh.draw();
}

void Model::shutdown()
{
    for (auto& mesh : meshes)
        mesh.shutdown();
    meshes.clear();
}

// ── Private helpers ─────────────────────────────────────────────────────────

void Model::processNode(aiNode* node, const aiScene* scene)
{
    // Process each mesh attached to this node
    for (unsigned int i = 0; i < node->mNumMeshes; ++i)
    {
        aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
        meshes.push_back(processMesh(mesh, scene));
    }

    // Recurse into children
    for (unsigned int i = 0; i < node->mNumChildren; ++i)
    {
        processNode(node->mChildren[i], scene);
    }
}

Mesh Model::processMesh(aiMesh* mesh, const aiScene* /*scene*/)
{
    std::vector<Vertex>        vertices;
    std::vector<std::uint32_t> indices;

    vertices.reserve(mesh->mNumVertices);

    // ── Vertices ────────────────────────────────────────────────────────
    for (unsigned int i = 0; i < mesh->mNumVertices; ++i)
    {
        Vertex vertex{};

        // Position (always present)
        vertex.Position.x = mesh->mVertices[i].x;
        vertex.Position.y = mesh->mVertices[i].y;
        vertex.Position.z = mesh->mVertices[i].z;

        // Normals (guaranteed by aiProcess_GenNormals)
        if (mesh->HasNormals())
        {
            vertex.Normal.x = mesh->mNormals[i].x;
            vertex.Normal.y = mesh->mNormals[i].y;
            vertex.Normal.z = mesh->mNormals[i].z;
        }

        // Texture coords — only the first UV channel (index 0)
        if (mesh->mTextureCoords[0])
        {
            vertex.TexCoords.x = mesh->mTextureCoords[0][i].x;
            vertex.TexCoords.y = mesh->mTextureCoords[0][i].y;
        }
        else
        {
            vertex.TexCoords = glm::vec2(0.0f, 0.0f);
        }

        vertices.push_back(vertex);
    }

    // ── Indices ─────────────────────────────────────────────────────────
    for (unsigned int i = 0; i < mesh->mNumFaces; ++i)
    {
        const aiFace& face = mesh->mFaces[i];
        for (unsigned int j = 0; j < face.mNumIndices; ++j)
        {
            indices.push_back(face.mIndices[j]);
        }
    }

    return Mesh(std::move(vertices), std::move(indices));
}

} // namespace engine
