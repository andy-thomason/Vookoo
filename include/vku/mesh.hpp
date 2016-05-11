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
          glm::vec3 apos = a.vtx.pos();
          glm::vec3 bpos = b.vtx.pos();
          return memcmp(&apos, &bpos, sizeof(apos)) < 0;
        }
      );

      size_t imax = expanded.size();
      for (size_t i = 0, idx = 0; i != imax; ) {
        auto &v0 = expanded[i];
        glm::vec3 normal = v0.vtx.normal();
        glm::vec3 pos = v0.vtx.pos();
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

  // Generate an implicit mesh from a function (ie. marching cubes).
  // Vertices will be generated where the function changes sign.
  template<class Function, class Generator>
  mesh(int xdim, int ydim, int zdim, Function fn, Generator vertex_generator) {
    // Now build the marching cubes triangles.
    // Each cube owns three edges 0->1 0->3 0->4
    std::vector<int> edge_indices(xdim*ydim*zdim*3);

    for (size_t i = 0; i != edge_indices.size(); ++i) {
      edge_indices [i] = -1;
    }

    for (int k = 0; k != zdim; ++k) {
      for (int j = 0; j != ydim; ++j) {
        for (int i = 0; i != xdim; ++i) {
          int idx = (k * ydim + j) * xdim + i;
          float fi = (float)i;
          float fj = (float)j;
          float fk = (float)k;
          float v0 = fn(fi, fj, fk);

          // x edges
          if (i != xdim-1) {
            float v1 = fn(fi+1, fj, fk);
            if (v0 < 0 != v1 < 0) {
              float lambda = v0 / (v0 - v1);
              edge_indices[idx*3+0] = (int)vertices_.size();
              vertices_.push_back(vertex_generator(fi + lambda, fj, fk));
            }
          }

          // y edges
          if (j != ydim-1) {
            float v1 = fn(fi, fj+1, fk);
            if (v0 < 0 != v1 < 0) {
              float lambda = v0 / (v0 - v1);
              edge_indices[idx*3+1] = (int)vertices_.size();
              vertices_.push_back(vertex_generator(fi, fj + lambda, fk));
            }
          }

          // z edges
          if (k != zdim-1) {
            float v1 = fn(fi, fj, fk+1);
            if (v0 < 0 != v1 < 0) {
              float lambda = v0 / (v0 - v1);
              edge_indices[idx*3+2] = (int)vertices_.size();
              vertices_.push_back(vertex_generator(fi, fj, fk + lambda));
            }
          }
        }
      }
    }

    // This reproduced the vertex order of Paul Bourke's (borrowed) table.
    // The indices in edge_indices have the following offsets.
    //
    //     7 6   y   z
    // 3 2 4 5   | /
    // 0 1       0 - x
    //

    // We store three indices per cube for the edges closest to vertex 0
    // All other indices can be derived from adjacent cubes.
    // This gives a single index offset value for each edge.
    // There are twelve edges here because we consider adjacent cubes also.
    int dx = 3;
    int dy = xdim * 3;
    int dz = xdim * ydim * 3;
    int edge_offsets[] = {
      0*dx+0*dy+0*dz+0,  // 0,1,
      1*dx+0*dy+0*dz+1,  // 1,2,
      0*dx+1*dy+0*dz+0,  // 2,3,
      0*dx+0*dy+0*dz+1,  // 3,0,
      0*dx+0*dy+1*dz+0,  // 4,5,
      1*dx+0*dy+1*dz+1,  // 5,6,
      0*dx+1*dy+1*dz+0,  // 6,7,
      0*dx+0*dy+1*dz+1,  // 7,4,
      0*dx+0*dy+0*dz+2,  // 0,4,
      1*dx+0*dy+0*dz+2,  // 1,5,
      1*dx+1*dy+0*dz+2,  // 2,6,
      0*dx+1*dy+0*dz+2,  // 3,7
    };
        
    // loop over all cubes
    for (int k = 0; k != zdim-1; ++k) {
      for (int j = 0; j != ydim-1; ++j) {
        for (int i = 0; i != xdim-1; ++i) {
          int idx = (k * ydim + j) * xdim + i;
          float fi = (float)i;
          float fj = (float)j;
          float fk = (float)k;

          // Mask of vertices outside the isosurface (values are negative)
          // Example:
          //   00000001 means only vertex 0 is outside the surface.
          //   10000000 means only vertex 7 is outside the surface.
          //   11111111 all vertices are outside the surface.
          float v000 = fn(fi, fj, fk);
          float v100 = fn(fi+1, fj, fk);
          float v010 = fn(fi, fj+1, fk);
          float v110 = fn(fi+1, fj+1, fk);
          float v001 = fn(fi, fj, fk+1);
          float v101 = fn(fi+1, fj, fk+1);
          float v011 = fn(fi, fj+1, fk+1);
          float v111 = fn(fi+1, fj+1, fk+1);

          int mask = (
            (v000 < 0 ? 1 << 0 : 0) |
            (v100 < 0 ? 1 << 1 : 0) |
            (v110 < 0 ? 1 << 2 : 0) |
            (v010 < 0 ? 1 << 3 : 0) |
          
            (v001 < 0 ? 1 << 4 : 0) |
            (v101 < 0 ? 1 << 5 : 0) |
            (v111 < 0 ? 1 << 6 : 0) |
            (v011 < 0 ? 1 << 7 : 0)
          );

          uint64_t triangles = mc_triangles()[mask];
          while ((triangles >> 60) != 0xc) {
            int t0 = triangles >> 60;
            triangles <<= 4;
            int t1 = triangles >> 60;
            triangles <<= 4;
            int t2 = triangles >> 60;
            triangles <<= 4;
            int i0 = edge_indices [idx*3 + edge_offsets [t0]];
            int i1 = edge_indices [idx*3 + edge_offsets [t1]];
            int i2 = edge_indices [idx*3 + edge_offsets [t2]];
            if (i0 >= 0 && i1 >= 0 && i2 >= 0) {
              indices_.push_back((index_t)i0);
              indices_.push_back((index_t)i1);
              indices_.push_back((index_t)i2);
            }
          }
        }
      }
    }
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

  static const uint64_t *mc_triangles() {
    // marching cubes edge lists
    // see http://paulbourke.net/geometry/polygonise/marchingsource.cpp for original.
    // this is much more compact.
    static const uint64_t values[] = {
      0xCCCCCCCCCCCCCCCCull,0x083CCCCCCCCCCCCCull,0x019CCCCCCCCCCCCCull,0x183981CCCCCCCCCCull,0x12ACCCCCCCCCCCCCull,0x08312ACCCCCCCCCCull,0x92A029CCCCCCCCCCull,0x2832A8A98CCCCCCCull,
      0x3B2CCCCCCCCCCCCCull,0x0B28B0CCCCCCCCCCull,0x19023BCCCCCCCCCCull,0x1B219B98BCCCCCCCull,0x3A1BA3CCCCCCCCCCull,0x0A108A8BACCCCCCCull,0x3903B9BA9CCCCCCCull,0x98AA8BCCCCCCCCCCull,
      0x478CCCCCCCCCCCCCull,0x430734CCCCCCCCCCull,0x019847CCCCCCCCCCull,0x419471731CCCCCCCull,0x12A847CCCCCCCCCCull,0x34730412ACCCCCCCull,0x92A902847CCCCCCCull,0x2A9297273794CCCCull,
      0x8473B2CCCCCCCCCCull,0xB47B24204CCCCCCCull,0x90184723BCCCCCCCull,0x47B94B9B2921CCCCull,0x3A13BA784CCCCCCCull,0x1BA14B1047B4CCCCull,0x47890B9BAB03CCCCull,0x47B4B99BACCCCCCCull,
      0x954CCCCCCCCCCCCCull,0x954083CCCCCCCCCCull,0x054150CCCCCCCCCCull,0x854835315CCCCCCCull,0x12A954CCCCCCCCCCull,0x30812A495CCCCCCCull,0x52A542402CCCCCCCull,0x2A5325354348CCCCull,
      0x95423BCCCCCCCCCCull,0x0B208B495CCCCCCCull,0x05401523BCCCCCCCull,0x21525828B485CCCCull,0xA3BA13954CCCCCCCull,0x4950818A18BACCCCull,0x54050B5BAB03CCCCull,0x54858AA8BCCCCCCCull,
      0x978579CCCCCCCCCCull,0x930953573CCCCCCCull,0x078017157CCCCCCCull,0x153357CCCCCCCCCCull,0x978957A12CCCCCCCull,0xA12950530573CCCCull,0x802825857A52CCCCull,0x2A5253357CCCCCCCull,
      0x7957893B2CCCCCCCull,0x95797292027BCCCCull,0x23B018178157CCCCull,0xB21B17715CCCCCCCull,0x958857A13A3BCCCCull,0x5705097B010ABA0Cull,0xBA0B03A50807570Cull,0xBA57B5CCCCCCCCCCull,
      0xA65CCCCCCCCCCCCCull,0x0835A6CCCCCCCCCCull,0x9015A6CCCCCCCCCCull,0x1831985A6CCCCCCCull,0x165261CCCCCCCCCCull,0x165126308CCCCCCCull,0x965906026CCCCCCCull,0x598582526328CCCCull,
      0x23BA65CCCCCCCCCCull,0xB08B20A65CCCCCCCull,0x01923B5A6CCCCCCCull,0x5A61929B298BCCCCull,0x63B653513CCCCCCCull,0x08B0B50515B6CCCCull,0x3B6036065059CCCCull,0x65969BB98CCCCCCCull,
      0x5A6478CCCCCCCCCCull,0x43047365ACCCCCCCull,0x1905A6847CCCCCCCull,0xA65197173794CCCCull,0x612651478CCCCCCCull,0x125526304347CCCCull,0x847905065026CCCCull,0x739794329596269Cull,
      0x3B2784A65CCCCCCCull,0x5A647242027BCCCCull,0x01947823B5A6CCCCull,0x9219B294B7B45A6Cull,0x8473B53515B6CCCCull,0x51B5B610B7B404BCull,0x059065036B63847Cull,0x65969B4797B9CCCCull,
      0xA4964ACCCCCCCCCCull,0x4A649A083CCCCCCCull,0xA01A60640CCCCCCCull,0x83181686461ACCCCull,0x149124264CCCCCCCull,0x308129249264CCCCull,0x024426CCCCCCCCCCull,0x832824426CCCCCCCull,
      0xA49A64B23CCCCCCCull,0x08228B49A4A6CCCCull,0x3B201606461ACCCCull,0x64161A48121B8B1Cull,0x964936913B63CCCCull,0x8B1810B61914641Cull,0x3B6360064CCCCCCCull,0x648B68CCCCCCCCCCull,
      0x7A678A89ACCCCCCCull,0x0730A709A67ACCCCull,0xA671A7178180CCCCull,0xA67A71173CCCCCCCull,0x126168189867CCCCull,0x269291679093739Cull,0x780706602CCCCCCCull,0x732672CCCCCCCCCCull,      0x23BA68A89867CCCCull,0x20727B09767A9A7Cull,0x1801781A767A23BCull,0xB21B17A61671CCCCull,0x896867916B63136Cull,0x091B67CCCCCCCCCCull,0x7807063B0B60CCCCull,0x7B6CCCCCCCCCCCCCull,
      0x76BCCCCCCCCCCCCCull,0x308B76CCCCCCCCCCull,0x019B76CCCCCCCCCCull,0x819831B76CCCCCCCull,0xA126B7CCCCCCCCCCull,0x12A3086B7CCCCCCCull,0x2902A96B7CCCCCCCull,0x6B72A3A83A98CCCCull,
      0x723627CCCCCCCCCCull,0x708760620CCCCCCCull,0x276237019CCCCCCCull,0x162186198876CCCCull,0xA76A17137CCCCCCCull,0xA7617A187108CCCCull,0x03707A0A96A7CCCCull,0x76A7A88A9CCCCCCCull,
      0x684B86CCCCCCCCCCull,0x36B306046CCCCCCCull,0x86B846901CCCCCCCull,0x946963931B36CCCCull,0x6846B82A1CCCCCCCull,0x12A30B06B046CCCCull,0x4B846B0292A9CCCCull,0xA93A32943B36463Cull,
      0x823842462CCCCCCCull,0x042462CCCCCCCCCCull,0x190234246438CCCCull,0x194142246CCCCCCCull,0x8138618466A1CCCCull,0xA10A06604CCCCCCCull,0x4634386A3039A93Cull,0xA946A4CCCCCCCCCCull,
      0x49576BCCCCCCCCCCull,0x083495B76CCCCCCCull,0x50154076BCCCCCCCull,0xB76834354315CCCCull,0x954A1276BCCCCCCCull,0x6B712A083495CCCCull,0x76B54A42A402CCCCull,0x348354325A52B76Cull,
      0x723762549CCCCCCCull,0x954086062687CCCCull,0x362376150540CCCCull,0x628687218485158Cull,0x954A16176137CCCCull,0x16A176107870954Cull,0x40A4A503A6A737ACull,0x76A7A854A48ACCCCull,
      0x6956B9B89CCCCCCCull,0x36B063056095CCCCull,0x0B805B01556BCCCCull,0x6B3635531CCCCCCCull,0x12A95B9B8B56CCCCull,0x0B306B09656912ACull,0xB85B56805A52025Cull,0x6B36352A3A53CCCCull,
      0x589528562382CCCCull,0x956960062CCCCCCCull,0x158180568382628Cull,0x156216CCCCCCCCCCull,0x13616A386569896Cull,0xA10A06950560CCCCull,0x03856ACCCCCCCCCCull,0xA56CCCCCCCCCCCCCull,
      0xB5A75BCCCCCCCCCCull,0xB5AB75830CCCCCCCull,0x5B75AB190CCCCCCCull,0xA75AB7981831CCCCull,0xB12B71751CCCCCCCull,0x08312717572BCCCCull,0x9759279022B7CCCCull,0x75272B592328982Cull,
      0x25A235375CCCCCCCull,0x820852875A25CCCCull,0x9015A35373A2CCCCull,0x982921872A25752Cull,0x135375CCCCCCCCCCull,0x087071175CCCCCCCull,0x903935537CCCCCCCull,0x987597CCCCCCCCCCull,
      0x5845A8AB8CCCCCCCull,0x5045B05ABB30CCCCull,0x01984A8ABA45CCCCull,0xAB4A45B34941314Cull,0x2512852B8458CCCCull,0x04B0B345B2B151BCull,0x0250592B5458B85Cull,0x9452B3CCCCCCCCCCull,
      0x25A352345384CCCCull,0x5A2524420CCCCCCCull,0x3A235A385458019Cull,0x5A2524192942CCCCull,0x845853351CCCCCCCull,0x045105CCCCCCCCCCull,0x845853905035CCCCull,0x945CCCCCCCCCCCCCull,
      0x4B749B9ABCCCCCCCull,0x0834979B79ABCCCCull,0x1AB1B414074BCCCCull,0x3143481A474BAB4Cull,0x4B79B492B912CCCCull,0x9749B791B2B1083Cull,0xB74B42240CCCCCCCull,0xB74B42834324CCCCull,
      0x29A279237749CCCCull,0x9A7974A27870207Cull,0x37A3A274A1A040ACull,0x1A2874CCCCCCCCCCull,0x491417713CCCCCCCull,0x491417081871CCCCull,0x403743CCCCCCCCCCull,0x487CCCCCCCCCCCCCull,
      0x9A8AB8CCCCCCCCCCull,0x30939BB9ACCCCCCCull,0x01A0A88ABCCCCCCCull,0x31AB3ACCCCCCCCCCull,0x12B1B99B8CCCCCCCull,0x30939B1292B9CCCCull,0x02B80BCCCCCCCCCCull,0x32BCCCCCCCCCCCCCull,
      0x23828AA89CCCCCCCull,0x9A2092CCCCCCCCCCull,0x23828A0181A8CCCCull,0x1A2CCCCCCCCCCCCCull,0x138918CCCCCCCCCCull,0x091CCCCCCCCCCCCCull,0x038CCCCCCCCCCCCCull,0xCCCCCCCCCCCCCCCCull,
    };
    return values;
  }

  std::vector<vertex_t> vertices_;
  std::vector<index_t> indices_;
};

struct simple_mesh_traits {
  class vertex_t {
  public:
    vertex_t() {}

    vertex_t(const glm::vec3 &pos, const glm::vec3 &normal, const glm::vec2 &uv) {
      pos_ = pos;
      normal_ = normal;
      uv_ = uv;
    }

    static void getVertexFormat(vku::pipelineCreateHelper &pipeHelper, uint32_t vertex_buffer_bind_id) {
      pipeHelper.binding(vertex_buffer_bind_id, sizeof(vertex_t), VK_VERTEX_INPUT_RATE_VERTEX);
      pipeHelper.attrib(0, vertex_buffer_bind_id, VK_FORMAT_R32G32B32_SFLOAT, 0);
      pipeHelper.attrib(1, vertex_buffer_bind_id, VK_FORMAT_R32G32B32_SFLOAT, sizeof(vertex_t::pos_));
      pipeHelper.attrib(2, vertex_buffer_bind_id, VK_FORMAT_R32G32_SFLOAT, sizeof(vertex_t::pos_) + sizeof(vertex_t::normal_));
    }

    glm::vec3 pos() const { return pos_; }
    glm::vec3 normal() const { return normal_; }
    glm::vec2 uv() const { return uv_; }

    vertex_t pos(const glm::vec3 &value) { pos_ = value; return *this; }
    vertex_t normal(const glm::vec3 &value) { normal_ = value; return *this; }
    vertex_t uv(const glm::vec2 &value) { uv_ = value; return *this; }
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
