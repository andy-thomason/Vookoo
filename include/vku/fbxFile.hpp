////////////////////////////////////////////////////////////////////////////////
//
// (C) Andy Thomason 2016
//
// Vookoo: fbx file decoder
// 

#ifndef VKU_FBXFILE_INCLUDED
#define VKU_FBXFILE_INCLUDED

#include <iostream>
#include <fstream>
#include <string>
//#include <filesystem>
#include <vector>

#include <vku/zipDecoder.hpp>
#include <glm/glm.hpp>

// see https://code.blender.org/2013/08/fbx-binary-file-format-specification/
// and https://banexdevblog.wordpress.com/2014/06/23/a-quick-tutorial-about-the-fbx-ascii-format/

namespace vku {

  class fbxFile {
    enum { debug = 0 };

    static inline std::uint8_t u1(const char *p) {
      const unsigned char *q = (const unsigned char *)p;
      std::uint32_t res = q[0];
      return res;
    }

    static inline std::uint16_t u2(const char *p) {
      const unsigned char *q = (const unsigned char *)p;
      std::uint32_t res = q[0] + q[1] * 0x100;
      return res;
    }

    static inline std::uint32_t u4(const char *p) {
      const unsigned char *q = (const unsigned char *)p;
      std::uint32_t res = q[0] + q[1] * 0x100 + q[2] * 0x10000 + q[3] * 0x1000000;
      return res;
    }

    static inline std::uint64_t u8(const char *p) {
      return ((std::uint64_t)u4(p+4) << 32) | u4(p);
    }

    class prop;

    class props {
    public:
      props(const char *begin=nullptr, size_t offset=0) : begin_(begin), offset(offset) {}
      prop begin() const { return prop(begin_, offset + 13 + len()); }
      prop end() const { return prop(begin_, offset + 13 + property_list_len() + len()); }

    private:
      size_t end_offset() const { return u4(begin_ + offset); }
      size_t num_properties() const { return u4(begin_ + offset + 4); }
      size_t property_list_len() const { return u4(begin_ + offset + 8); }
      std::uint8_t len() const { return u1(begin_ + offset + 12); }

      size_t offset;
      const char *begin_;
    };

    // http://code.blender.org/2013/08/fbx-binary-file-format-specification/
    class node {
    public:
      node(const char *begin=nullptr, size_t offset=0) : begin_(begin), offset_(offset) {}
      bool operator !=(node &rhs) { return offset_ != rhs.offset_; }
      node &operator++() { offset_ = end_offset(); return *this; }
      std::string name() const { return std::string(begin_ + offset_ + 13, begin_ + offset_ + 13 + len()); }
      node begin() const { size_t new_offset = offset_ + 13 + property_list_len() + len(), end = end_offset(); return node(begin_, new_offset == end ? end-13 : new_offset); }
      node end() const { return node(begin_, end_offset() - 13); }
      node &operator*() { return *this; }
      fbxFile::props get_props() { return fbxFile::props(begin_, offset_); }

      size_t offset() const { return offset_; }
      size_t end_offset() const { return u4(begin_ + offset_); }
      size_t num_properties() const { return u4(begin_ + offset_ + 4); }
      size_t property_list_len() const { return u4(begin_ + offset_ + 8); }
      std::uint8_t len() const { return u1(begin_ + offset_ + 12); }

    //private:
      size_t offset_;
      const char *begin_;
    };

    class prop {
    public:
      prop(const char *begin=nullptr, size_t offset=0) : begin_(begin), offset(offset) {}
      bool operator !=(prop &rhs) { return offset != rhs.offset; }
      prop &operator*() { return *this; }
      char kind() const { return begin_[offset]; }

      prop &operator++() {
        const char *p = begin_ + offset;
        size_t al, enc, cl;
        switch(*p++) {
            case 'Y': p += 2; break;
            case 'C': p += 1; break;
            case 'I': p += 4; break;
            case 'F': p += 4; break;
            case 'D': p += 8; break;
            case 'L': p += 8; break;
            case 'f': al = u4(p); enc = u4(p+4); cl = u4(p+8); p += 12; p += !enc ? al * 4 : cl; break;
            case 'd': al = u4(p); enc = u4(p+4); cl = u4(p+8); p += 12; p += !enc ? al * 8 : cl; break;
            case 'l': al = u4(p); enc = u4(p+4); cl = u4(p+8); p += 12; p += !enc ? al * 8 : cl; break;
            case 'i': al = u4(p); enc = u4(p+4); cl = u4(p+8); p += 12; p += !enc ? al * 4 : cl; break;
            case 'b': al = u4(p); enc = u4(p+4); cl = u4(p+8); p += 12; p += !enc ? al * 1 : cl; break;
            case 'S': al = u4(p); p += 4 + al; break;
            case 'R': al = u4(p); p += 4 + al; break;
            default: throw std::runtime_error("bad fbx property"); break;
        }
        offset = p - begin_;
        return *this;
      }

      operator std::string() {
        const char *p = begin_ + offset;
        size_t al;
        static char tmp[65536];
        int fv; std::uint64_t dv;
        switch(*p++) {
            case 'Y': sprintf(tmp, "%d", (short)u2(p)); break;
            case 'C': sprintf(tmp, *p ? "true" : "false"); break;
            case 'I': sprintf(tmp, "%d", (std::int32_t)u4(p)); break;
            case 'F': fv = u4(p); sprintf(tmp, "%8f", (float&)(fv)); break;
            case 'D': dv = u8(p); sprintf(tmp, "%10f", (double&)(dv)); break;
            case 'L': sprintf(tmp, "%lld", (long long)u8(p)); break;
            case 'f': return "<array>"; break;
            case 'd': return "<array>"; break;
            case 'l': return "<array>"; break;
            case 'i': return "<array>"; break;
            case 'b': return "<array>"; break;
            case 'S': al = u4(p); return std::string(p+4, p + 4 + al);
            case 'R': al = u4(p); return "<raw>";
            default: throw std::runtime_error("bad fbx property"); break;
        }
        return tmp;
      }

      bool getString(std::string &result) {
        if (kind() == 'S') {
          const char *p = begin_ + offset + 1;
          size_t al = u4(p);
          result.assign(p+4, p+4+al);
          return true;
        }
        return false;
      }

      template <class Type, char Kind>
      bool getArray(std::vector<Type> &result, zipDecoder &decoder) const {
        Type *begin = nullptr;
        Type *end = nullptr;
        if (kind() == Kind) {
          const char *p = begin_ + offset + 1;
          size_t al = u4(p);
          size_t enc = u4(p+4);
          size_t cl = u4(p+8);
          p += 12;
          result.resize(al);
          uint8_t *dest = (uint8_t *)result.data();
          uint8_t *dest_max = (uint8_t *)(result.data() + result.size());
          if (enc) {
            const uint8_t *src = (const uint8_t *)p;
            const uint8_t *src_max = (const uint8_t *)p + cl;
            // bytes 0 and 1 are the ZLIB code.
            // see http://stackoverflow.com/questions/9050260/what-does-a-zlib-header-look-like
            if ((src[0] & 0x0f) == 0x08) {
              return decoder.decode(dest, dest_max, src+2, src_max);
            }
          } else {
            memcpy(dest, p, al*8);
            return true;
          }
        }
        return false;
      }
    private:
      size_t offset;
      const char *begin_;
    };
  public:
    fbxFile(const std::string &filename) {
      std::ifstream f(filename, std::ios_base::binary);
      if (f.good()) {
        f.seekg(0, std::ios_base::end);
        bytes.resize((size_t)f.tellg());
        f.seekg(0, std::ios_base::beg);
        f.read((char*)bytes.data(), bytes.size());
        init((char*)bytes.data(), (char*)bytes.data() + f.gcount());
      }
    }

    fbxFile(const char *begin, const char *end) { init(begin, end); }

    node begin() const { return node(begin_, 27); }
    node end() const { return node(begin_, end_offset); }

    enum class Mapping {
      Invalid,
      ByPolygon,
      ByPolygonVertex,
      ByVertex,
      ByEdge,
      AllSame,
    };

    enum class Ref {
      Invalid,
      Direct,
      IndexToDirect,
    };

    static Mapping decodeMapping(const std::string &name) {
      Mapping result = Mapping::Invalid;
      if (name == "ByPolygon") result = Mapping::ByPolygon;
      else if (name == "ByPolygon") result = Mapping::ByPolygon;
      else if (name == "ByPolygonVertex") result = Mapping::ByPolygonVertex;
      else if (name == "ByVertex") result = Mapping::ByVertex;
      else if (name == "ByVertice") result = Mapping::ByVertex;
      else if (name == "ByEdge") result = Mapping::ByEdge;
      else if (name == "AllSame") result = Mapping::AllSame;
      return result;
    };

    static Ref decodeRef(const std::string &name) {
      Ref result = Ref::Invalid;
      if (name == "Direct") result = Ref::Direct;
      else if (name == "IndexToDirect") result = Ref::IndexToDirect;
      else if (name == "Index") result = Ref::IndexToDirect;
      return result;
    };

    template <class MeshTraits>
    bool loadFirstMesh(std::vector<typename MeshTraits::vertex_t> &vertices, std::vector<typename MeshTraits::index_t> &indices) {
      typedef typename MeshTraits::vertex_t vertex_t;
      typedef typename MeshTraits::index_t index_t;

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

      for (auto section : *this) {
        if (section.name() == "Objects") {
          for (auto obj : section) {
            if (obj.name() == "Geometry") {
              for (auto comp : obj) {
                auto vp = comp.get_props().begin();
                if (debug) printf("%s %c\n", comp.name().c_str(), vp.kind());
                if (comp.name() == "Vertices") {
                  vp.getArray<double, 'd'>(fbxVertices, decoder);
                } else if (comp.name() == "LayerElementNormal") {
                  for (auto sub : comp) {
                    auto vp = sub.get_props().begin();
                    if (debug) printf("  %s %c\n", sub.name().c_str(), vp.kind());
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
                    auto vp = sub.get_props().begin();
                    if (debug) printf("  %s %c\n", sub.name().c_str(), vp.kind());
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

              if (debug) printf("%s %s\n", fbxNormalMapping.c_str(), fbxUVMapping.c_str());
              if (debug) printf("%s %s\n", fbxNormalRef.c_str(), fbxUVRef.c_str());
              if (debug) printf("%d vertices %d indices %d normals %d uvs %d uvindices\n", (int)fbxVertices.size(), (int)fbxIndices.size(), (int)fbxNormals.size(), (int)fbxUVs.size(), (int)fbxUVIndices.size());

              // map the fbx data to real vertices
              for (size_t i = 0; i != fbxIndices.size(); ++i) {
                size_t ni = normalRef == fbxFile::Ref::IndexToDirect ? fbxNormalIndices[i] : i;
                size_t uvi = uvRef == fbxFile::Ref::IndexToDirect ? fbxUVIndices[i] : i;
                int32_t vi = fbxIndices[i];
                if (vi < 0) vi = -1 - vi;

                glm::vec3 pos(fbxVertices[vi*3+0], fbxVertices[vi*3+1], fbxVertices[vi*3+2]);
                glm::vec3 normal(1, 0, 0);
                glm::vec2 uv(0, 0);
                if (normalMapping == fbxFile::Mapping::ByPolygonVertex) {
                  normal = glm::vec3(fbxNormals[ni*3+0], fbxNormals[ni*3+1], fbxNormals[ni*3+2]);
                }
                if (uvMapping == fbxFile::Mapping::ByPolygonVertex) {
                  uv = glm::vec2(fbxUVs[uvi*2+0], fbxUVs[uvi*2+1]);
                }
                vertices.emplace_back(pos, normal, uv);
              }

              // map the fbx data to real indices
              // todo: add a function to re-index
              for (size_t i = 0, j = 0; i != fbxIndices.size(); ++i) {
                if (fbxIndices[i] < 0) {
                  for (size_t k = j+2; k <= i; ++k) {
                    indices.push_back((uint32_t)j);
                    indices.push_back((uint32_t)k-1);
                    indices.push_back((uint32_t)k);
                  }
                  j = i + 1;
                }
              }
              return true;
            } // if (obj.name() == "Geometry")
          }
        } // if (section.name() == "Objects")
      }
      return false;
    }

  private:

    void init(const char *begin, const char *end) {
      begin_ = begin;
      end_ = end;

      if (end < begin + 27+4 || memcmp(begin, "Kaydara FBX Binary  ", 20)) {
        bad_fbx();
      }

      const char *p = begin + 23;
      std::string text;

      std::vector <const char *> ends;
      std::uint32_t version = u4(p);
      p += 4;

      while (u4(p)) {
        p = begin + u4(p);
      }
      end_offset = p - begin;
    }

    void dump(std::ostream &os, node &n, int depth) const {
      char tmp[256];
      snprintf(tmp, sizeof(tmp), "%*s%08zx..%08zx %s\n", depth*2, "", n.offset(), n.end_offset(), n.name().c_str());
      os << tmp;
      for (auto p : n.get_props()) {
          snprintf(tmp, sizeof(tmp), "%*s   %c %s\n", depth*2, "", p.kind(), ((std::string)p).c_str());
          os << tmp;
      }
      for (auto child : n) {
          dump(os, child, depth+1);
      }
    }

    friend std::ostream &operator<<(std::ostream &os, const fbxFile &fbx);

    void bad_fbx() { throw std::runtime_error("bad fbx"); }

    std::vector<std::uint8_t> bytes;
    //scene the_scene;
    size_t end_offset;
    const char *begin_;
    const char *end_;
  };

  inline std::ostream &operator<<(std::ostream &os, const fbxFile &fbx) {
    for (auto p : fbx) {
      fbx.dump(os, p, 0);
    }
    return os;
  }

} // vku

#endif
