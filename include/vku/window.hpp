////////////////////////////////////////////////////////////////////////////////
//
// (C) Andy Thomason 2016
//
// Vookoo: window class: basic framework for booting vulkan
// 

#ifndef VKU_WINDOW_INCLUDED
#define VKU_WINDOW_INCLUDED


#include <vku/queue.hpp>
#include <vku/semaphore.hpp>
#include <vku/image.hpp>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <unordered_map>

// derived from https://github.com/SaschaWillems/Vulkan
//
// Many thanks to Sascha, without who this would be a challenge!

namespace vku {

  inline float deg_to_rad(float deg) { return deg * (3.1415927f / 180); }

  #ifdef VK_USE_PLATFORM_WIN32_KHR
    template <class WindowHandle, class Window> Window *map_window(WindowHandle handle, Window *win) {
      static std::unordered_map<WindowHandle, Window *> map;
      auto iter = map.find(handle);
      if (iter == map.end()) {
        if (win != nullptr) map[handle] = win;
        return win;
      } else {
        return iter->second;
      }
    };

    inline static HINSTANCE connection() { return GetModuleHandle(NULL); }
  #else
    inline static xcb_connection_t *connection() {
      static xcb_connection_t *connection = nullptr;
      if (connection == nullptr) {

        const xcb_setup_t *setup;
        xcb_screen_iterator_t iter;
        int scr = 10;

        setup = xcb_get_setup(connection);
        iter = xcb_setup_roots_iterator(setup);
        while (scr-- > 0) {
          xcb_screen_next(&iter);
          connection = xcb_connect(NULL, &scr);
          if (connection == NULL) {
            printf("Could not find a compatible Vulkan ICD!\n");
            fflush(stdout);
            exit(1);
          }
        }
      }
      return connection;
    }
  #endif

  #ifdef VK_USE_PLATFORM_WIN32_KHR
    template <class Window> static LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
    {
      Window *win = map_window<HWND, window>(hWnd, nullptr);
      //printf("%04x %p %p\n", uMsg, hWnd, win);
      if (win) win->handleMessages(hWnd, uMsg, wParam, lParam);
      return (DefWindowProc(hWnd, uMsg, wParam, lParam));
    }
  #endif


class window {
public:
  window(int argc, const char **argv, bool enableValidation, uint32_t width, uint32_t height, float zoom, const std::string &title) :
    width_(width), height_(height), zoom_(zoom), title_(title), argc_(argc), argv_(argv)
  {
    for (int32_t i = 0; i < argc; i++) {
      if (argv[i] == std::string("-validation")) {
        enableValidation = true;
      }
    }

    device_ = instance::singleton().device();
    queue_ = instance::singleton().queue();

    // Find a suitable depth format
    depthFormat_ = device_.getSupportedDepthFormat();
    assert(depthFormat_ != VK_FORMAT_UNDEFINED);

    setupWindow();
    prepareWindow();
  }

  ~window() {
    // Clean up Vulkan resources
    swapChain_.clear();

    cmdPool_.clear();

    pipelineCache_.clear();

    cmdPool_.clear();

    if (enableValidation_)
    {
      //vkDebug::freeDebugCallback(instance);
    }

    //instance_.clear();

    #ifndef _WIN32
      xcb_destroy_window(connection(), window_);
      //xcb_disconnect(connection);
    #endif 
  }

  #ifdef _WIN32 
    HWND setupWindow()
    {
      bool fullscreen = false;

      // Check command line arguments
      for (int32_t i = 0; i < argc_; i++)
      {
        if (argv_[i] == std::string("-fullscreen"))
        {
          fullscreen = true;
        }
      }

      WNDCLASSEX wndClass;

      wndClass.cbSize = sizeof(WNDCLASSEX);
      wndClass.style = CS_HREDRAW | CS_VREDRAW;
      wndClass.lpfnWndProc = WndProc<vku::window>;
      wndClass.cbClsExtra = 0;
      wndClass.cbWndExtra = 0;
      wndClass.hInstance = connection();
      wndClass.hIcon = LoadIcon(NULL, IDI_APPLICATION);
      wndClass.hCursor = LoadCursor(NULL, IDC_ARROW);
      wndClass.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
      wndClass.lpszMenuName = NULL;
      wndClass.lpszClassName = name_.c_str();
      wndClass.hIconSm = LoadIcon(NULL, IDI_WINLOGO);

      RegisterClassEx(&wndClass);

      int screenWidth = GetSystemMetrics(SM_CXSCREEN);
      int screenHeight = GetSystemMetrics(SM_CYSCREEN);

      if (fullscreen)
      {
        DEVMODE dmScreenSettings;
        memset(&dmScreenSettings, 0, sizeof(dmScreenSettings));
        dmScreenSettings.dmSize = sizeof(dmScreenSettings);
        dmScreenSettings.dmPelsWidth = screenWidth;
        dmScreenSettings.dmPelsHeight = screenHeight;
        dmScreenSettings.dmBitsPerPel = 32;
        dmScreenSettings.dmFields = DM_BITSPERPEL | DM_PELSWIDTH | DM_PELSHEIGHT;

        if ((width_ != screenWidth) && (height_ != screenHeight))
        {
          if (ChangeDisplaySettings(&dmScreenSettings, CDS_FULLSCREEN) != DISP_CHANGE_SUCCESSFUL)
          {
            if (MessageBox(NULL, "Fullscreen Mode not supported!\n Switch to window mode?", "Error", MB_YESNO | MB_ICONEXCLAMATION) == IDYES)
            {
              fullscreen = FALSE;
            }
            else
            {
              return FALSE;
            }
          }
        }

      }

      DWORD dwExStyle;
      DWORD dwStyle;

      if (fullscreen)
      {
        dwExStyle = WS_EX_APPWINDOW;
        dwStyle = WS_POPUP | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;
      }
      else
      {
        dwExStyle = WS_EX_APPWINDOW | WS_EX_WINDOWEDGE;
        dwStyle = WS_OVERLAPPEDWINDOW | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;
      }

      RECT windowRect;
      if (fullscreen)
      {
        windowRect.left = (long)0;
        windowRect.right = (long)screenWidth;
        windowRect.top = (long)0;
        windowRect.bottom = (long)screenHeight;
      }
      else
      {
        windowRect.left = (long)screenWidth / 2 - width_ / 2;
        windowRect.right = (long)width_;
        windowRect.top = (long)screenHeight / 2 - height_ / 2;
        windowRect.bottom = (long)height_;
      }

      AdjustWindowRectEx(&windowRect, dwStyle, FALSE, dwExStyle);

      window_ = CreateWindowEx(0,
        name_.c_str(),
        title_.c_str(),
        dwStyle | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
        windowRect.left,
        windowRect.top,
        windowRect.right,
        windowRect.bottom,
        NULL,
        NULL,
        connection(),
        NULL);

      if (!window_) 
      {
        printf("Could not create window!\n");
        fflush(stdout);
        return 0;
        exit(1);
      }

      map_window(window_, this);

      ShowWindow(window_, SW_SHOW);
      SetForegroundWindow(window_);
      SetFocus(window_);

      return window_;
    }

    void handleMessages(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
    {
      switch (uMsg)
      {
      case WM_CLOSE:
        prepared = false;
        DestroyWindow(hWnd);
        map_window(hWnd, (window*)VK_NULL_HANDLE);
        windowIsClosed_ = true;
        //PostQuitMessage(0);
        break;
      case WM_PAINT:
        ValidateRect(window_, NULL);
        break;
      case WM_KEYDOWN:
        switch (wParam)
        {
        case 0x50:
          paused_ = !paused_;
          break;
        case VK_ESCAPE:
          exit(0);
          break;
        }
        break;
      case WM_RBUTTONDOWN:
      case WM_LBUTTONDOWN:
        mousePos_.x = (float)LOWORD(lParam);
        mousePos_.y = (float)HIWORD(lParam);
        break;
      case WM_MOUSEMOVE:
        if (wParam & MK_RBUTTON)
        {
          int32_t posx = LOWORD(lParam);
          int32_t posy = HIWORD(lParam);
          zoom_ += (mousePos_.y - (float)posy) * .005f * zoomSpeed_;
          mousePos_ = glm::vec2((float)posx, (float)posy);
          viewChanged();
        }
        if (wParam & MK_LBUTTON)
        {
          int32_t posx = LOWORD(lParam);
          int32_t posy = HIWORD(lParam);
          rotation_.x += (mousePos_.y - (float)posy) * 1.25f * rotationSpeed_;
          rotation_.y -= (mousePos_.x - (float)posx) * 1.25f * rotationSpeed_;
          mousePos_ = glm::vec2((float)posx, (float)posy);
          viewChanged();
        }
        break;
      }
    }
  #else // WIN32
    // Linux : Setup window 
    // TODO : Not finished...
    xcb_window_t setupWindow()
    {
      uint32_t value_mask, value_list[32];

      window_ = xcb_generate_id(connection());

      value_mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
      value_list[0] = screen->black_pixel;
      value_list[1] = XCB_EVENT_MASK_KEY_RELEASE |
        XCB_EVENT_MASK_EXPOSURE |
        XCB_EVENT_MASK_STRUCTURE_NOTIFY |
        XCB_EVENT_MASK_POINTER_MOTION |
        XCB_EVENT_MASK_BUTTON_PRESS |
        XCB_EVENT_MASK_BUTTON_RELEASE;

      xcb_create_window(connection(),
        XCB_COPY_FROM_PARENT,
        window_, screen->root,
        0, 0, width_, height_, 0,
        XCB_WINDOW_CLASS_INPUT_OUTPUT,
        screen->root_visual,
        value_mask, value_list);

      /* Magic code that will send notification when window is destroyed */
      xcb_intern_atom_cookie_t cookie = xcb_intern_atom(connection(), 1, 12, "WM_PROTOCOLS");
      xcb_intern_atom_reply_t* reply = xcb_intern_atom_reply(connection(), cookie, 0);

      xcb_intern_atom_cookie_t cookie2 = xcb_intern_atom(connection(), 0, 16, "WM_DELETE_WINDOW");
      atom_wm_delete_window = xcb_intern_atom_reply(connection(), cookie2, 0);

      xcb_change_property(connection(), XCB_PROP_MODE_REPLACE,
        window_, (*reply).atom, 4, 32, 1,
        &(*atom_wm_delete_window).atom);

      xcb_change_property(connection(), XCB_PROP_MODE_REPLACE,
        window_, XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 8,
        title_.size(), title_.c_str());

      free(reply);

      xcb_map_window(connection(), window_);

      return(window_);
    }

/*
    // Initialize XCB connection
    void initxcbConnection()
    {
      const xcb_setup_t *setup;
      xcb_screen_iterator_t iter;
      int scr;

      setup = xcb_get_setup(connection());
      iter = xcb_setup_roots_iterator(setup);
      while (scr-- > 0)
        xcb_screen_next(&iter);
      screen = iter.data;
    }
*/

    void handleEvent(const xcb_generic_event_t *event)
    {
      switch (event->response_type & 0x7f)
      {
      case XCB_CLIENT_MESSAGE:
        if ((*(xcb_client_message_event_t*)event).data.data32[0] ==
          (*atom_wm_delete_window).atom) {
          windowIsClosed_ = true;
        }
        break;
      case XCB_MOTION_NOTIFY:
      {
        xcb_motion_notify_event_t *motion = (xcb_motion_notify_event_t *)event;
        if (mouseButtons_.left)
        {
          rotation_.x += (mousePos_.y - (float)motion->event_y) * 1.25f;
          rotation_.y -= (mousePos_.x - (float)motion->event_x) * 1.25f;
          viewChanged();
        }
        if (mouseButtons_.right)
        {
          zoom_ += (mousePos_.y - (float)motion->event_y) * .005f;
          viewChanged();
        }
        mousePos_ = glm::vec2((float)motion->event_x, (float)motion->event_y);
      }
      break;
      case XCB_BUTTON_PRESS:
      {
        xcb_button_press_event_t *press = (xcb_button_press_event_t *)event;
        mouseButtons_.left = (press->detail & XCB_BUTTON_INDEX_1);
        mouseButtons_.right = (press->detail & XCB_BUTTON_INDEX_3);
      }
      break;
      case XCB_BUTTON_RELEASE:
      {
        xcb_button_press_event_t *press = (xcb_button_press_event_t *)event;
        if (press->detail & XCB_BUTTON_INDEX_1)
          mouseButtons_.left = false;
        if (press->detail & XCB_BUTTON_INDEX_3)
          mouseButtons_.right = false;
      }
      break;
      case XCB_KEY_RELEASE:
      {
        const xcb_key_release_event_t *key =
          (const xcb_key_release_event_t *)event;

        if (key->detail == 0x9) {
          windowIsClosed_ = true;
        }
      }
      break;
      case XCB_DESTROY_NOTIFY:
        windowIsClosed_ = true;
        break;
      default:
        break;
      }
    }
  #endif

  virtual void viewChanged()
  {
    // For overriding on derived class
  }


  void prepareWindow() {
    VkSurfaceKHR surface = createSurface(instance::singleton().get(), (void*)(intptr_t)window_, connection());
    uint32_t queueNodeIndex = device_.getGraphicsQueueNodeIndex(surface);
    if (queueNodeIndex == ~(uint32_t)0) throw(std::runtime_error("no graphics and present queue available"));
    auto sf = device_.getSurfaceFormat(surface);
    //swapChain.colorFormat = sf.first;
    //swapChain.colorSpace = sf.second;

    if (enableValidation_) {
      //vkDebug::setupDebugging(instance, VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT, NULL);
    }

    cmdPool_ = vku::commandPool(device_, queueNodeIndex);

    setupCmdBuffer_ = vku::commandBuffer(device_, cmdPool_);
    setupCmdBuffer_.beginCommandBuffer();

    swapChain_ = vku::swapChain(device_, width_, height_, surface, setupCmdBuffer_);
    width_ = swapChain_.width();
    height_ = swapChain_.height();

    assert(swapChain_.imageCount() <= 2);

    for (size_t i = 0; i != swapChain_.imageCount(); ++i) {
      drawCmdBuffers_[i] = vku::commandBuffer(device_, cmdPool_);
    }

    postPresentCmdBuffer_ = vku::commandBuffer(device_, cmdPool_);

    imageLayoutHelper layout(width_, height_);
    layout.format(depthFormat_);
    layout.tiling(VK_IMAGE_TILING_OPTIMAL);
    layout.usage(VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
    layout.initialLayout(VK_IMAGE_LAYOUT_UNDEFINED);

    depthStencil_ = vku::image(device_, layout);
    //depthStencil_.allocate(device_);
    //depthStencil_.bindMemoryToImage();
    depthStencil_.setImageLayout(setupCmdBuffer_, VK_IMAGE_ASPECT_DEPTH_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
    depthStencil_.createView(layout);

    pipelineCache_ = vku::pipelineCache(device_);
    //createPipelineCache();

    swapChain_.setupFrameBuffer(depthStencil_.view(), depthFormat_);

    setupCmdBuffer_.endCommandBuffer();
    queue_.submit(VK_NULL_HANDLE, setupCmdBuffer_);
    queue_.waitIdle();

    // Recreate setup command buffer for derived class

    setupCmdBuffer_ = vku::commandBuffer(device_, cmdPool_);
    setupCmdBuffer_.beginCommandBuffer();

    // Create a simple texture loader class 
    //textureLoader = new vkTools::VulkanTextureLoader(instance_.physicalDevice(), device, queue, cmdPool);
  }



  static bool poll() {
  #ifdef _WIN32
    MSG msg;
    PeekMessage(&msg, NULL, 0, 0, PM_REMOVE);
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  #else
    xcb_flush(connection());
    xcb_generic_event_t *event = xcb_poll_for_event(connection());
    if (event) {
      vku::window *win = nullptr;
      win->handleEvent(event);
      free(event);
    }
  #endif
    return true;
  }

  glm::mat4 defaultProjectionMatrix() const {
    return glm::perspective(deg_to_rad(60.0f), (float)width() / (float)height(), 0.1f, 256.0f);
  }

  glm::mat4 defaultViewMatrix() const {
    glm::mat4 m;
    m = glm::translate(m, glm::vec3(0.0f, 0.0f, zoom()));
    m = glm::rotate(m, deg_to_rad(rotation().x), glm::vec3(1.0f, 0.0f, 0.0f));
    m = glm::rotate(m, deg_to_rad(rotation().y), glm::vec3(0.0f, 1.0f, 0.0f));
    m = glm::rotate(m, deg_to_rad(rotation().z), glm::vec3(0.0f, 0.0f, 1.0f));
    //return glm::translate(glm::mat4(), glm::vec3(0.0f, 0.0f, zoom()));
    return m;
  }

  glm::mat4 defaultModelMatrix() const {
    glm::mat4 m;
    m = glm::rotate(m, deg_to_rad(rotation().x), glm::vec3(1.0f, 0.0f, 0.0f));
    m = glm::rotate(m, deg_to_rad(rotation().y), glm::vec3(0.0f, 1.0f, 0.0f));
    m = glm::rotate(m, deg_to_rad(rotation().z), glm::vec3(0.0f, 0.0f, 1.0f));
    return m;
  }

  void present() {
    {
      vku::semaphore sema(device_);

      // Get next image in the swap chain (back/front buffer)
      currentBuffer_ = swapChain_.acquireNextImage(sema);

      queue_.submit(sema, drawCmdBuffers_[currentBuffer_]);
    }

    // Present the current buffer to the swap chain
    // This will display the image
    swapChain_.present(queue_, currentBuffer());

    postPresentCmdBuffer_.beginCommandBuffer();
    postPresentCmdBuffer_.addPostPresentBariier(swapChain_.image(currentBuffer()));
    postPresentCmdBuffer_.endCommandBuffer();

    queue_.submit(VK_NULL_HANDLE, postPresentCmdBuffer_);

    queue_.waitIdle();
  }

  const vku::device &device() const { return device_; }
  const vku::queue &queue() const { return queue_; }
  const vku::commandPool &cmdPool() const { return cmdPool_; }
  const vku::commandBuffer &setupCmdBuffer() const { return setupCmdBuffer_; }
  const vku::commandBuffer &postPresentCmdBuffer() const { return postPresentCmdBuffer_; }
  const vku::commandBuffer &drawCmdBuffer(size_t i) const { return drawCmdBuffers_[i]; }
  const vku::pipelineCache &pipelineCache() const { return pipelineCache_; }
  const vku::image &depthStencil() const { return depthStencil_; }
  const vku::swapChain &swapChain() const { return swapChain_; }

  const VkFormat colorformat() const { return colorformat_; }
  const VkFormat depthFormat() const { return depthFormat_; }
  const uint32_t currentBuffer() const { return currentBuffer_; }
  const uint32_t width() const { return width_; }
  const uint32_t height() const { return height_; }
  const float frameTimer() const { return frameTimer_; }
  const VkClearColorValue &defaultClearColor() const { return defaultClearColor_; }
  const float zoom() const { return zoom_; }
  const float timer() const { return timer_; }
  const float timerSpeed() const { return timerSpeed_; }
  const bool paused() const { return paused_; }
  const bool windowIsClosed() const { return windowIsClosed_; }
  const bool enableValidation() const { return enableValidation_; }
  const float rotationSpeed() const { return rotationSpeed_; }
  const float zoomSpeed() const { return zoomSpeed_; }
  const glm::vec3 &rotation() const { return rotation_; }
  const glm::vec2 &mousePos() const { return mousePos_; }
  const std::string &title() const { return title_; }
  const std::string &name() const { return name_; }
public:
  virtual void render() = 0;

private:
  static VkSurfaceKHR createSurface(VkInstance instance, void *window, void *connection) {
    VkSurfaceKHR result = VK_NULL_HANDLE;
    // Create surface depending on OS
    #if defined(_WIN32)
      VkWin32SurfaceCreateInfoKHR surfaceCreateInfo = {};
      surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
      surfaceCreateInfo.hinstance = (HINSTANCE)connection;
      surfaceCreateInfo.hwnd = (HWND)window;
      VkResult err = vkCreateWin32SurfaceKHR(instance, &surfaceCreateInfo, nullptr, &result);
    #elif defined(__ANDROID__)
      VkAndroidSurfaceCreateInfoKHR surfaceCreateInfo = {};
      surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
      surfaceCreateInfo.window = window;
      VkResult err = vkCreateAndroidSurfaceKHR(instance, &surfaceCreateInfo, nullptr, &result);
    #else
      VkXcbSurfaceCreateInfoKHR surfaceCreateInfo = {};
      surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_XCB_SURFACE_CREATE_INFO_KHR;
      surfaceCreateInfo.connection = connection;
      surfaceCreateInfo.window = (xcb_window_t)(intptr_t)window;
      VkResult err = vkCreateXcbSurfaceKHR(instance, &surfaceCreateInfo, nullptr, &result);
    #endif
    if (err) throw error(err, __FILE__, __LINE__);
    return result;
  }

  vku::device device_;
  vku::queue queue_;
  vku::commandPool cmdPool_;
  vku::commandBuffer setupCmdBuffer_;
  vku::commandBuffer postPresentCmdBuffer_;
  vku::commandBuffer drawCmdBuffers_[2];
  vku::pipelineCache pipelineCache_;
  vku::image depthStencil_;
  vku::swapChain swapChain_;

  VkFormat colorformat_ = VK_FORMAT_B8G8R8A8_UNORM;
  VkFormat depthFormat_;
  uint32_t currentBuffer_ = 0;
  bool prepared = false;
  uint32_t width_ = 1280;
  uint32_t height_ = 720;
  int argc_ = 0;
  const char **argv_ = nullptr;

  float frameTimer_ = 1.0f;
  VkClearColorValue defaultClearColor_ = { { 0.025f, 0.025f, 0.025f, 1.0f } };

  float zoom_ = 0;

  // Defines a frame rate independent timer value clamped from -1.0...1.0
  // For use in animations, rotations, etc.
  float timer_ = 0.0f;
  // Multiplier for speeding up (or slowing down) the global timer
  float timerSpeed_ = 0.25f;
  
  bool paused_ = false;
  bool windowIsClosed_ = false;
  bool enableValidation_ = false;

  // Use to adjust mouse rotation speed
  float rotationSpeed_ = 1.0f;
  // Use to adjust mouse zoom speed
  float zoomSpeed_ = 1.0f;

  glm::vec3 rotation_ = glm::vec3();
  glm::vec2 mousePos_;

  std::string title_ = "Vulkan Example";
  std::string name_ = "vulkanExample";

  // OS specific 
  #ifdef _WIN32
    HWND window_;
  #else
    struct {
      bool left = false;
      bool right = false;
    } mouseButtons_;

    xcb_screen_t *screen;
    xcb_window_t window_;
    xcb_intern_atom_reply_t *atom_wm_delete_window;
  #endif  
};



} // vku

#endif
