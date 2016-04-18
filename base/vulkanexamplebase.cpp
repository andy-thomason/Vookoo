/*
* Vulkan Example base class
*
* Copyright (C) 2015 by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#include "vulkanexamplebase.h"

void VulkanExampleBase::prepare()
{
  vku::device dev(device, instance.physicalDevice());

  VkSurfaceKHR surface = instance.createSurface((void*)windowInstance, (void*)window);
  uint32_t queueNodeIndex = dev.getGraphicsQueueNodeIndex(surface);
  if (queueNodeIndex == ~(uint32_t)0) throw(std::runtime_error("no graphics and present queue available"));
  auto sf = dev.getSurfaceFormat(surface);
  //swapChain.colorFormat = sf.first;
  //swapChain.colorSpace = sf.second;

	if (enableValidation)
	{
		//vkDebug::setupDebugging(instance, VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT, NULL);
	}

  cmdPool = vku::commandPool(device, queueNodeIndex);

  setupCmdBuffer = vku::cmdBuffer(device, cmdPool);
  setupCmdBuffer.beginCommandBuffer();

  swapChain = vku::swapChain(vku::device(device, instance.physicalDevice()), width, height, surface, setupCmdBuffer);
  width = swapChain.width();
  height = swapChain.height();

  assert(swapChain.imageCount() <= 2);

  for (size_t i = 0; i != swapChain.imageCount(); ++i) {
    drawCmdBuffers[i] = vku::cmdBuffer(device, cmdPool);
  }

  postPresentCmdBuffer = vku::cmdBuffer(device, cmdPool);

  depthStencil = vku::image(device, width, height, depthFormat, VK_IMAGE_TYPE_2D, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
  depthStencil.allocate(dev);
  depthStencil.bindMemoryToImage();
	depthStencil.setImageLayout(setupCmdBuffer, VK_IMAGE_ASPECT_DEPTH_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
  depthStencil.createView();

  pipelineCache = vku::pipelineCache(device);
	//createPipelineCache();

  swapChain.setupFrameBuffer(depthStencil.view(), depthFormat);

  setupCmdBuffer.endCommandBuffer();
  vku::queue q(queue, device);
  q.submit(nullptr, setupCmdBuffer);
  q.waitIdle();

	// Recreate setup command buffer for derived class

  setupCmdBuffer = vku::cmdBuffer(device, cmdPool);
  setupCmdBuffer.beginCommandBuffer();

	// Create a simple texture loader class 
	//textureLoader = new vkTools::VulkanTextureLoader(instance.physicalDevice(), device, queue, cmdPool);
}


void VulkanExampleBase::renderLoop()
{
#ifdef _WIN32
	MSG msg;
	while (TRUE)
	{
		auto tStart = std::chrono::high_resolution_clock::now();
		PeekMessage(&msg, NULL, 0, 0, PM_REMOVE);
		if (msg.message == WM_QUIT)
		{
			break;
		}
		else
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		render();
		auto tEnd = std::chrono::high_resolution_clock::now();
		auto tDiff = std::chrono::duration<double, std::milli>(tEnd - tStart).count();
		frameTimer = (float)tDiff / 1000.0f;
		// Convert to clamped timer value
		if (!paused)
		{
			timer += timerSpeed * frameTimer;
			if (timer > 1.0)
			{
				timer -= 1.0f;
			}
		}
	}
#else
	xcb_flush(connection);
	while (!quit)
	{
		auto tStart = std::chrono::high_resolution_clock::now();
		xcb_generic_event_t *event;
		event = xcb_poll_for_event(connection);
		if (event) 
		{
			handleEvent(event);
			free(event);
		}
		render();
		auto tEnd = std::chrono::high_resolution_clock::now();
		auto tDiff = std::chrono::duration<double, std::milli>(tEnd - tStart).count();
		frameTimer = tDiff / 1000.0f;
}
#endif
}

// todo : comment
/*void VulkanExampleBase::submitPostPresentBarrier(VkImage image)
{
  postPresentCmdBuffer.addPostPresentationBarrier(image);
  postPresentCmdBuffer.endCommandBuffer();

	VkSubmitInfo submitInfo = {};
  VkCommandBuffer ppcb = postPresentCmdBuffer;
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &ppcb;

	VkResult vkRes = vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
	assert(!vkRes);

	vkRes = vkQueueWaitIdle(queue);
	assert(!vkRes);
}*/

VulkanExampleBase::VulkanExampleBase(bool enableValidation)
{
	// Check for validation command line flag
#ifdef _WIN32
	for (int32_t i = 0; i < __argc; i++)
	{
		if (__argv[i] == std::string("-validation"))
		{
			enableValidation = true;
		}
	}
#endif

#ifndef _WIN32
	initxcbConnection();
#endif
	initVulkan(enableValidation);
	// Enable console if validation is active
	// Debug message callback will output to it
#ifdef _WIN32 
	if (enableValidation)
	{
		setupConsole("VulkanExample");
	}
#endif
}

VulkanExampleBase::~VulkanExampleBase()
{
	// Clean up Vulkan resources
	swapChain.clear();

  cmdPool.clear();

  pipelineCache.clear();

  cmdPool.clear();

	//vkDestroyDevice(device, nullptr); 
  //device.clear();

	if (enableValidation)
	{
		//vkDebug::freeDebugCallback(instance);
	}

  instance.clear();

#ifndef _WIN32
	xcb_destroy_window(connection, window);
	xcb_disconnect(connection);
#endif 
}

void VulkanExampleBase::initVulkan(bool enableValidation)
{
  instance = vku::instance("vku");

  vku::device dev = instance.device();
  device = dev;
  queue = instance.queue();

	// Find a suitable depth format
	depthFormat = dev.getSupportedDepthFormat();
	assert(depthFormat != VK_FORMAT_UNDEFINED);
}

#ifdef _WIN32 
// Win32 : Sets up a console window and redirects standard output to it
void VulkanExampleBase::setupConsole(std::string title)
{
	AllocConsole();
	AttachConsole(GetCurrentProcessId());
	freopen("CON", "w", stdout);
	SetConsoleTitle(TEXT(title.c_str()));
	if (enableValidation)
	{
		std::cout << "Validation enabled:\n";
	}
}

HWND VulkanExampleBase::setupWindow(HINSTANCE hinstance, WNDPROC wndproc)
{
	this->windowInstance = hinstance;

	bool fullscreen = false;

	// Check command line arguments
	for (int32_t i = 0; i < __argc; i++)
	{
		if (__argv[i] == std::string("-fullscreen"))
		{
			fullscreen = true;
		}
	}

	WNDCLASSEX wndClass;

	wndClass.cbSize = sizeof(WNDCLASSEX);
	wndClass.style = CS_HREDRAW | CS_VREDRAW;
	wndClass.lpfnWndProc = wndproc;
	wndClass.cbClsExtra = 0;
	wndClass.cbWndExtra = 0;
	wndClass.hInstance = hinstance;
	wndClass.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	wndClass.hCursor = LoadCursor(NULL, IDC_ARROW);
	wndClass.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
	wndClass.lpszMenuName = NULL;
	wndClass.lpszClassName = name.c_str();
	wndClass.hIconSm = LoadIcon(NULL, IDI_WINLOGO);

	if (!RegisterClassEx(&wndClass))
	{
		std::cout << "Could not register window class!\n";
		fflush(stdout);
		exit(1);
	}

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

		if ((width != screenWidth) && (height != screenHeight))
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
		windowRect.left = (long)screenWidth / 2 - width / 2;
		windowRect.right = (long)width;
		windowRect.top = (long)screenHeight / 2 - height / 2;
		windowRect.bottom = (long)height;
	}

	AdjustWindowRectEx(&windowRect, dwStyle, FALSE, dwExStyle);

	window = CreateWindowEx(0,
		name.c_str(),
		title.c_str(),
		//		WS_OVERLAPPEDWINDOW | WS_VISIBLE | WS_SYSMENU,
		dwStyle | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
		windowRect.left,
		windowRect.top,
		windowRect.right,
		windowRect.bottom,
		NULL,
		NULL,
		hinstance,
		NULL);

	if (!window) 
	{
		printf("Could not create window!\n");
		fflush(stdout);
		return 0;
		exit(1);
	}

	ShowWindow(window, SW_SHOW);
	SetForegroundWindow(window);
	SetFocus(window);

	return window;
}

void VulkanExampleBase::handleMessages(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_CLOSE:
		prepared = false;
		DestroyWindow(hWnd);
		PostQuitMessage(0);
		break;
	case WM_PAINT:
		ValidateRect(window, NULL);
		break;
	case WM_KEYDOWN:
		switch (wParam)
		{
		case 0x50:
			paused = !paused;
			break;
		case VK_ESCAPE:
			exit(0);
			break;
		}
		break;
	case WM_RBUTTONDOWN:
	case WM_LBUTTONDOWN:
		mousePos.x = (float)LOWORD(lParam);
		mousePos.y = (float)HIWORD(lParam);
		break;
	case WM_MOUSEMOVE:
		if (wParam & MK_RBUTTON)
		{
			int32_t posx = LOWORD(lParam);
			int32_t posy = HIWORD(lParam);
			zoom += (mousePos.y - (float)posy) * .005f * zoomSpeed;
			mousePos = glm::vec2((float)posx, (float)posy);
			viewChanged();
		}
		if (wParam & MK_LBUTTON)
		{
			int32_t posx = LOWORD(lParam);
			int32_t posy = HIWORD(lParam);
			rotation.x += (mousePos.y - (float)posy) * 1.25f * rotationSpeed;
			rotation.y -= (mousePos.x - (float)posx) * 1.25f * rotationSpeed;
			mousePos = glm::vec2((float)posx, (float)posy);
			viewChanged();
		}
		break;
	}
}

#else

// Linux : Setup window 
// TODO : Not finished...
xcb_window_t VulkanExampleBase::setupWindow()
{
	uint32_t value_mask, value_list[32];

	window = xcb_generate_id(connection);

	value_mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
	value_list[0] = screen->black_pixel;
	value_list[1] = XCB_EVENT_MASK_KEY_RELEASE |
		XCB_EVENT_MASK_EXPOSURE |
		XCB_EVENT_MASK_STRUCTURE_NOTIFY |
		XCB_EVENT_MASK_POINTER_MOTION |
		XCB_EVENT_MASK_BUTTON_PRESS |
		XCB_EVENT_MASK_BUTTON_RELEASE;

	xcb_create_window(connection,
		XCB_COPY_FROM_PARENT,
		window, screen->root,
		0, 0, width, height, 0,
		XCB_WINDOW_CLASS_INPUT_OUTPUT,
		screen->root_visual,
		value_mask, value_list);

	/* Magic code that will send notification when window is destroyed */
	xcb_intern_atom_cookie_t cookie = xcb_intern_atom(connection, 1, 12, "WM_PROTOCOLS");
	xcb_intern_atom_reply_t* reply = xcb_intern_atom_reply(connection, cookie, 0);

	xcb_intern_atom_cookie_t cookie2 = xcb_intern_atom(connection, 0, 16, "WM_DELETE_WINDOW");
	atom_wm_delete_window = xcb_intern_atom_reply(connection, cookie2, 0);

	xcb_change_property(connection, XCB_PROP_MODE_REPLACE,
		window, (*reply).atom, 4, 32, 1,
		&(*atom_wm_delete_window).atom);

	xcb_change_property(connection, XCB_PROP_MODE_REPLACE,
		window, XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 8,
		title.size(), title.c_str());

	free(reply);

	xcb_map_window(connection, window);

	return(window);
}

// Initialize XCB connection
void VulkanExampleBase::initxcbConnection()
{
	const xcb_setup_t *setup;
	xcb_screen_iterator_t iter;
	int scr;

	connection = xcb_connect(NULL, &scr);
	if (connection == NULL) {
		printf("Could not find a compatible Vulkan ICD!\n");
		fflush(stdout);
		exit(1);
	}

	setup = xcb_get_setup(connection);
	iter = xcb_setup_roots_iterator(setup);
	while (scr-- > 0)
		xcb_screen_next(&iter);
	screen = iter.data;
}

void VulkanExampleBase::handleEvent(const xcb_generic_event_t *event)
{
	switch (event->response_type & 0x7f)
	{
	case XCB_CLIENT_MESSAGE:
		if ((*(xcb_client_message_event_t*)event).data.data32[0] ==
			(*atom_wm_delete_window).atom) {
			quit = true;
		}
		break;
	case XCB_MOTION_NOTIFY:
	{
		xcb_motion_notify_event_t *motion = (xcb_motion_notify_event_t *)event;
		if (mouseButtons.left)
		{
			rotation.x += (mousePos.y - (float)motion->event_y) * 1.25f;
			rotation.y -= (mousePos.x - (float)motion->event_x) * 1.25f;
			viewChanged();
		}
		if (mouseButtons.right)
		{
			zoom += (mousePos.y - (float)motion->event_y) * .005f;
			viewChanged();
		}
		mousePos = glm::vec2((float)motion->event_x, (float)motion->event_y);
	}
	break;
	case XCB_BUTTON_PRESS:
	{
		xcb_button_press_event_t *press = (xcb_button_press_event_t *)event;
		mouseButtons.left = (press->detail & XCB_BUTTON_INDEX_1);
		mouseButtons.right = (press->detail & XCB_BUTTON_INDEX_3);
	}
	break;
	case XCB_BUTTON_RELEASE:
	{
		xcb_button_press_event_t *press = (xcb_button_press_event_t *)event;
		if (press->detail & XCB_BUTTON_INDEX_1)
			mouseButtons.left = false;
		if (press->detail & XCB_BUTTON_INDEX_3)
			mouseButtons.right = false;
	}
	break;
	case XCB_KEY_RELEASE:
	{
		const xcb_key_release_event_t *key =
			(const xcb_key_release_event_t *)event;

		if (key->detail == 0x9)
			quit = true;
	}
	break;
	case XCB_DESTROY_NOTIFY:
		quit = true;
		break;
	default:
		break;
	}
}
#endif

void VulkanExampleBase::viewChanged()
{
	// For overriding on derived class
}
