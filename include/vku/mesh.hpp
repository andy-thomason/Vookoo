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
template <class MeshTraits>
class mesh {
public:
  typedef MeshTraits traits_t;
  typedef typename MeshTraits::vertex_t vertex_t;
  typedef typename MeshTraits::index_t index_t;

  // empty mesh
  mesh() {
  }

  // load an FBX file
  mesh(const std::string &filename) {
    fbxFile fbx(filename);
    fbx.loadFirstMesh<traits_t>(vertices_, indices_);
  }

  const vertex_t *vertices() const { return vertices_.data(); }
  size_t numVertices() const { return vertices_.size(); }
  size_t vertexSize() const { return sizeof(vertex_t); }

  const index_t *indices() const { return indices_.data(); }
  size_t numIndices() const { return indices_.size(); }
  size_t indexSize() const { return sizeof(index_t); }

  mesh &operator=(mesh &&rhs) {
    vertices_ = std::move(rhs.vertices_);
    indices_ = std::move(rhs.indices_);
    return *this;
  }

  void getVertexFormat(vku::pipelineCreateHelper &pipeHelper, uint32_t vertex_buffer_bind_id) const {
    return MeshTraits::getVertexFormat(pipeHelper, vertex_buffer_bind_id);
  }
private:
  std::vector<vertex_t> vertices_;
  std::vector<uint32_t> indices_;
};

struct simple_mesh_traits {
  struct vertex_t {
    glm::vec3 pos;
    glm::vec3 normal;
    glm::vec2 uv;
  };

  typedef uint32_t index_t;

  static void getVertexFormat(vku::pipelineCreateHelper &pipeHelper, uint32_t vertex_buffer_bind_id) {
    pipeHelper.binding(vertex_buffer_bind_id, sizeof(vertex_t), VK_VERTEX_INPUT_RATE_VERTEX);
    pipeHelper.attrib(0, vertex_buffer_bind_id, VK_FORMAT_R32G32B32_SFLOAT, 0);
    pipeHelper.attrib(1, vertex_buffer_bind_id, VK_FORMAT_R32G32B32_SFLOAT, sizeof(vertex_t::pos));
    pipeHelper.attrib(2, vertex_buffer_bind_id, VK_FORMAT_R32G32_SFLOAT, sizeof(vertex_t::pos) + sizeof(vertex_t::normal));
  }
};

typedef mesh<simple_mesh_traits> simple_mesh;

} // vku

#endif
