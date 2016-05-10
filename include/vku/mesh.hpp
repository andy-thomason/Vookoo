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
    return MeshTraits::vertex_t::getVertexFormat(pipeHelper, vertex_buffer_bind_id);
  }

  void reindex(bool recalcNormals = false) {
    std::vector<expandedVertex> expanded = expand();
    if (recalcNormals) {
      std::sort(
        expanded.begin(), expanded.end(),
        [](const expandedVertex &a, const expandedVertex &b) {
          glm::vec4 apos = a.vtx.pos();
          glm::vec4 bpos = b.vtx.pos();
          return memcmp(&apos, &bpos, sizeof(apos)) < 0;
        }
      );

      size_t imax = expanded.size();
      for (size_t i = 0, idx = 0; i != imax; ) {
        auto &v0 = expanded[i];
        glm::vec4 normal = v0.vtx.normal();
        glm::vec4 pos = v0.vtx.pos();
        size_t j = i + 1;
        for (; j < imax && pos == expanded[j].vtx.pos(); ++j) {
          normal += expanded[j].vtx.normal();
        }

        normal = glm::normalize(normal);

        while (i < j) {
          expanded[i++].vtx.normal(normal);
        }
      }
    }
    compact(expanded);
  }
private:
  struct expandedVertex {
    vertex_t vtx;
    size_t order;
  };

  // convert the indexed form to the expanded form.
  std::vector<expandedVertex> expand() const {
    std::vector<expandedVertex> result;
    result.reserve(indices_.size());
    for (size_t i = 0; i != indices_.size(); ++i) {
      result.push_back(expandedVertex{vertices_[indices_[i]], i});
    }
    return std::move(result);
  }

  // convert the expanded form to the indexed form.
  void compact(std::vector<expandedVertex> &expanded) {
    std::sort(
      expanded.begin(), expanded.end(),
      [](const expandedVertex &a, const expandedVertex &b) {
        return memcmp(&a.vtx, &b.vtx, sizeof(a.vtx)) < 0;
      }
    );

    vertices_.resize(0);
    size_t imax = expanded.size();
    for (size_t i = 0; i != imax; ) {
      auto &v0 = expanded[i];
      size_t idx = vertices_.size();
      vertices_.emplace_back(expanded[i].vtx);
      indices_[expanded[i].order] = (index_t)idx;
      for (++i; i < imax && !memcmp(&v0.vtx, &expanded[i].vtx, sizeof(v0.vtx)); ++i) {
        indices_[expanded[i].order] = (index_t)idx;
      }
    }
  }

  std::vector<vertex_t> vertices_;
  std::vector<index_t> indices_;
};

struct simple_mesh_traits {
  class vertex_t {
  public:
    vertex_t() {}

    vertex_t(const glm::vec4 &pos, const glm::vec4 &normal, const glm::vec4 &uv) {
      pos_ = glm::vec3(pos);
      normal_ = glm::vec3(normal);
      uv_ = glm::vec2(uv);
    }

    static void getVertexFormat(vku::pipelineCreateHelper &pipeHelper, uint32_t vertex_buffer_bind_id) {
      pipeHelper.binding(vertex_buffer_bind_id, sizeof(vertex_t), VK_VERTEX_INPUT_RATE_VERTEX);
      pipeHelper.attrib(0, vertex_buffer_bind_id, VK_FORMAT_R32G32B32_SFLOAT, 0);
      pipeHelper.attrib(1, vertex_buffer_bind_id, VK_FORMAT_R32G32B32_SFLOAT, sizeof(vertex_t::pos_));
      pipeHelper.attrib(2, vertex_buffer_bind_id, VK_FORMAT_R32G32_SFLOAT, sizeof(vertex_t::pos_) + sizeof(vertex_t::normal_));
    }

    glm::vec4 pos() const { return glm::vec4(pos_, 1); }
    glm::vec4 normal() const { return glm::vec4(normal_, 0); }
    glm::vec4 uv() const { return glm::vec4(uv_, 0, 1); }

    vertex_t pos(const glm::vec4 &value) { pos_ = glm::vec3(value); return *this; }
    vertex_t normal(const glm::vec4 &value) { normal_ = glm::vec3(value); return *this; }
    vertex_t uv(const glm::vec4 &value) { uv_ = glm::vec2(value); return *this; }
  private:
    glm::vec3 pos_;
    glm::vec3 normal_;
    glm::vec2 uv_;
  };

  typedef uint32_t index_t;

};

typedef mesh<simple_mesh_traits> simple_mesh;

} // vku

#endif
