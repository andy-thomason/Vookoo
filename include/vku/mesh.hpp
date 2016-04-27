////////////////////////////////////////////////////////////////////////////////
//
// (C) Andy Thomason 2016
//
// Vookoo: basic mesh object
// 

#ifndef VKU_MESH_INCLUDED
#define VKU_MESH_INCLUDED

#include <vku/vku.hpp>
#include <vku/fbxFile.hpp>
#include <vku/zipDecoder.hpp>
#include <glm/glm.hpp>
#include <vector>

namespace vku {

// A wrapper for meshes.
class mesh {
public:
  // todo: make this a template param?
  struct Vertex {
    glm::vec3 pos;
    glm::vec3 normal;
    glm::vec2 uv;
  };

  // empty mesh
  mesh() {
  }

  // load an FBX file
  mesh(const std::string &filename) {
    fbxFile fbx(filename);
    fbx.loadFirstMesh<Vertex>(vertices_, indices_);
  }

  const Vertex *vertices() const { return vertices_.data(); }
  size_t numVertices() const { return vertices_.size(); }

  const uint32_t *indices() const { return indices_.data(); }
  size_t numIndices() const { return indices_.size(); }

  mesh &operator=(mesh &&rhs) {
    vertices_ = std::move(rhs.vertices_);
    indices_ = std::move(rhs.indices_);
    return *this;
  }

  void getVertexFormat(vku::pipelineCreateHelper &pipeHelper, uint32_t vertex_buffer_bind_id) const {
    pipeHelper.binding(vertex_buffer_bind_id, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX);
    pipeHelper.attrib(0, vertex_buffer_bind_id, VK_FORMAT_R32G32B32_SFLOAT, 0);
    pipeHelper.attrib(1, vertex_buffer_bind_id, VK_FORMAT_R32G32B32_SFLOAT, sizeof(float) * 3);
    pipeHelper.attrib(2, vertex_buffer_bind_id, VK_FORMAT_R32G32_SFLOAT, sizeof(float) * 6);
  }
private:
  std::vector<Vertex> vertices_;
  std::vector<uint32_t> indices_;
};


} // vku

#endif
