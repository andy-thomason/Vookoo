
#ifndef VKU_INCLUDED
#define VKU_INCLUDED


namespace vku {

// A wrapper for meshes.
class mesh {
public:
  mesh() {
  }

  mesh(const std::string &filename) {
    // load a FBX file
  }

  void *mesh_data() const { return (void*)mesh_data_.data(); }
  uint32_t mesh_size() const { return (uint32_t)mesh_data_.size(); }
  void *index_data() const { return (void*)index_data_.data(); }
  uint32_t index_size() const { return (uint32_t)index_data_.size(); }
  uint32_t vertex_size() const {
    return std::accumulate(
      attribs_.begin(), attribs_.end(), (uint32_t)0,
      [](uint32_t a, const attrib &b) { return a + b.size; }
    );
  }

  void getVertexFormat(vku::pipelineCreateHelper &pipeHelper) const {
    //pipeHelper.binding(0, mesh.vertex_size(), VK_VERTEX_INPUT_RATE_VERTEX);
    //pipeHelper.attrib(0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0);
  }
private:
  struct attrib {
    VkFormat format;
    uint32_t size;
  };
  std::vector<attrib> attribs_;
  std::vector<uint8_t> mesh_data_;
  std::vector<uint8_t> index_data_;
};


} // vku

#endif
