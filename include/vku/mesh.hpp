////////////////////////////////////////////////////////////////////////////////
//
// (C) Andy Thomason 2016
//
// Vookoo: basic mesh object
// 

#ifndef VKU_MESH_INCLUDED
#define VKU_MESH_INCLUDED

#include <vku/fbxFile.hpp>
#include <vku/zipDecoder.hpp>
#include <glm/glm.hpp>

namespace vku {

// A wrapper for meshes.
class mesh {
public:
  mesh() {
  }

  // load an FBX file
  mesh(const std::string &filename) {
    fbxFile fbx(filename);
    vku::zipDecoder decoder;

    std::vector<double> fbxVertices;
    std::vector<double> fbxNormals;
    std::vector<double> fbxUVs;
    std::vector<int32_t> fbxUVIndices;
    std::vector<int32_t> fbxNormalIndices;
    std::vector<int32_t> fbxIndices;
    std::string fbxNormalMapping;
    std::string fbxUVMapping;
    std::string fbxNormalRef;
    std::string fbxUVRef;

    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    for (auto section : fbx) {
      if (section.name() == "Objects") {
        for (auto obj : section) {
          if (obj.name() == "Geometry") {
            for (auto comp : obj) {
              auto vp = comp.props().begin();
              printf("%s %c\n", comp.name().c_str(), vp.kind());
              if (comp.name() == "Vertices") {
                vp.getArray<double, 'd'>(fbxVertices, decoder);
              } else if (comp.name() == "LayerElementNormal") {
                for (auto sub : comp) {
                  auto vp = sub.props().begin();
                  printf("  %s %c\n", sub.name().c_str(), vp.kind());
                  if (sub.name() == "MappingInformationType") {
                    vp.getString(fbxNormalMapping);
                  } else if (sub.name() == "ReferenceInformationType") {
                    vp.getString(fbxNormalRef);
                  } else if (sub.name() == "NormalIndex") {
                    vp.getArray<int32_t, 'i'>(fbxNormalIndices, decoder);
                  } else if (sub.name() == "Normals") {
                    vp.getArray<double, 'd'>(fbxNormals, decoder);
                  }
                }
              } else if (comp.name() == "LayerElementUV") {
                for (auto sub : comp) {
                  auto vp = sub.props().begin();
                  printf("  %s %c\n", sub.name().c_str(), vp.kind());
                  if (sub.name() == "MappingInformationType") {
                    vp.getString(fbxUVMapping);
                  } else if (sub.name() == "ReferenceInformationType") {
                    vp.getString(fbxUVRef);
                  } else if (sub.name() == "UVIndex") {
                    vp.getArray<int32_t, 'i'>(fbxUVIndices, decoder);
                  } else if (sub.name() == "UV") {
                    vp.getArray<double, 'd'>(fbxUVs, decoder);
                  }
                }
              } else if (comp.name() == "PolygonVertexIndex") {
                vp.getArray<int32_t, 'i'>(fbxIndices, decoder);
              }
            }

            auto normalMapping = fbxFile::decodeMapping(fbxNormalMapping);
            auto uvMapping = fbxFile::decodeMapping(fbxUVMapping);
            auto normalRef = fbxFile::decodeRef(fbxNormalRef);
            auto uvRef = fbxFile::decodeRef(fbxUVRef);

            printf("%s %s\n", fbxNormalMapping.c_str(), fbxUVMapping.c_str());
            printf("%s %s\n", fbxNormalRef.c_str(), fbxUVRef.c_str());
            printf("%d vertices %d indices %d normals %d uvs %d uvindices\n", (int)fbxVertices.size(), (int)fbxIndices.size(), (int)fbxNormals.size(), (int)fbxUVs.size(), (int)fbxUVIndices.size());

            for (size_t i = 0; i != fbxIndices.size(); ++i) {
              size_t ni = normalRef == fbxFile::Ref::Direct ? i : fbxNormalIndices[i];
              size_t uvi = uvRef == fbxFile::Ref::Direct ? i : fbxUVIndices[i];
              size_t vi = std::abs(fbxIndices[i]);

              Vertex vtx = {};
              vtx.pos = glm::vec3(fbxVertices[vi*3+0], fbxVertices[vi*3+1], fbxVertices[vi*3+2]);
              if (normalMapping == fbxFile::Mapping::ByPolygonVertex) {
                vtx.normal = glm::vec3(fbxNormals[ni*3+0], fbxNormals[ni*3+1], fbxNormals[ni*3+2]);
              }
              if (uvMapping == fbxFile::Mapping::ByPolygonVertex) {
                vtx.uv = glm::vec2(fbxUVs[uvi*2+0], fbxUVs[uvi*2+1]);
              }
              vertices.push_back(vtx);
            }
          }
        }
      }
    }
  }

  void *mesh_data() const { return (void*)mesh_data_.data(); }
  uint32_t mesh_size() const { return (uint32_t)mesh_data_.size(); }

  void *index_data() const { return (void*)index_data_.data(); }
  uint32_t index_size() const { return (uint32_t)index_data_.size(); }

  void getVertexFormat(vku::pipelineCreateHelper &pipeHelper) const {
    //pipeHelper.binding(0, mesh.vertex_size(), VK_VERTEX_INPUT_RATE_VERTEX);
    //pipeHelper.attrib(0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0);
  }

  mesh &operator=(mesh &&rhs) {
    mesh_data_ = std::move(rhs.mesh_data_);
    index_data_ = std::move(rhs.index_data_);
    return *this;
  }
private:
  struct Vertex {
    glm::vec3 pos;
    glm::vec3 normal;
    glm::vec2 uv;
  };

  std::vector<uint8_t> mesh_data_;
  std::vector<uint8_t> index_data_;
};


} // vku

#endif
