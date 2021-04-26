#include "mu_gfx_impl.h"

#define NOMINMAX

#include <GLFW/glfw3.h>
#ifdef _WIN32
#undef APIENTRY
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h> // for glfwGetWin32Window
#endif

#include "mu_diligent.h"
#include "imgui_renderer.h"

namespace mu
{
	namespace details
	{
		struct glfw_system
		{
			int m_glfw_status = GLFW_FALSE;

			std::weak_ptr<diligent_globals> m_renderer_globals;

			glfw_system()
			{
				m_glfw_status = glfwInit();

				if (m_glfw_status != GLFW_TRUE) [[likely]]
				{
					MU_LEAF_THROW_EXCEPTION(gfx_error::not_specified{});
				}
			}

			virtual ~glfw_system()
			{
				if (m_glfw_status == GLFW_TRUE) [[likely]]
				{
					m_glfw_status = GLFW_FALSE;
					try
					{
						glfwTerminate();
					}
					catch (...)
					{
						MU_LEAF_LOG_ERROR(gfx_error::not_specified{});
					}
				}
			}
		};
	} // namespace details
} // namespace mu

namespace mu
{
	namespace details
	{
		struct imgui_context
		{
			std::shared_ptr<diligent_window>				 m_diligent_window;
			std::shared_ptr<ImGuiContext>					 m_imgui_context;
			std::shared_ptr<Diligent::imgui_renderer> m_imgui_renderer;
			float											 m_dpi_scale{1.0f};

			imgui_context(GLFWwindow* window, std::shared_ptr<diligent_window> diligent_window)
				: m_diligent_window(diligent_window)
				, m_imgui_context(ImGui::CreateContext(), ImGui::DestroyContext)
			{
				const auto& swapchain_desc = m_diligent_window->m_swap_chain->GetDesc();

				ImGui::SetCurrentContext(m_imgui_context.get());
				m_dpi_scale = get_dpi_scale_for_hwnd(glfwGetWin32Window(window));
				
				m_imgui_renderer = std::make_shared<Diligent::imgui_renderer>(
					m_diligent_window->m_globals->m_device,
					swapchain_desc.ColorBufferFormat,
					swapchain_desc.DepthBufferFormat,
					1024 * 1024,
					1024 * 1024,
					m_dpi_scale);

				ImGui::GetStyle().ScaleAllSizes(m_dpi_scale);
			}

			~imgui_context()
			{
				try
				{
					m_imgui_renderer.reset();
				}
				catch (...)
				{
					MU_LEAF_LOG_ERROR(mu::gfx_error::not_specified{});
				}

				try
				{
					m_imgui_context.reset();
				}
				catch (...)
				{
					MU_LEAF_LOG_ERROR(mu::gfx_error::not_specified{});
				}

				try
				{
					m_diligent_window.reset();
				}
				catch (...)
				{
					MU_LEAF_LOG_ERROR(mu::gfx_error::not_specified{});
				}
			}

			void make_current() noexcept
			{
				ImGui::SetCurrentContext(m_imgui_context.get());
			}
		};

		struct gfx_window_impl : public gfx_window
		{
			std::shared_ptr<glfw_system> m_glfw_system;
			GLFWwindow*					 m_window = nullptr;

			std::shared_ptr<diligent_window>  m_diligent_window;
			std::shared_ptr<diligent_globals> m_renderer_globals;
			std::shared_ptr<imgui_context>	  m_imgui_context;
			std::array<int, 2>				  m_display_size{0, 0};

			std::array<GLFWcursor*, ImGuiMouseCursor_COUNT> g_MouseCursors;
			std::array<bool, ImGuiMouseButton_COUNT>		g_MouseJustPressed;
			bool											g_WantUpdateMonitors{false};

			auto create_window(int posX, int posY, int sizeX, int sizeY) noexcept -> leaf::result<GLFWwindow*>
			try
			{
				glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
				glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_TRUE);
				if (auto wnd = glfwCreateWindow(sizeX, sizeY, "", NULL, NULL); wnd != nullptr) [[likely]]
				{
					try
					{
						glfwSetWindowPos(wnd, posX, posY);
						glfwGetFramebufferSize(wnd, &m_display_size[0], &m_display_size[1]);
						glfwSetWindowUserPointer(wnd, this);
						return wnd;
					}
					catch (...)
					{
						glfwDestroyWindow(wnd);
						return MU_LEAF_NEW_ERROR(gfx_error::not_specified{});
					}
				}

				return MU_LEAF_NEW_ERROR(gfx_error::not_specified{});
			}
			catch (...)
			{
				return MU_LEAF_NEW_ERROR(gfx_error::not_specified{});
			}

			auto destroy_window(GLFWwindow* wnd) noexcept -> leaf::result<void>
			try
			{
				if (wnd != nullptr) [[likely]]
				{
					glfwDestroyWindow(wnd);
					return {};
				}

				return MU_LEAF_NEW_ERROR(gfx_error::not_specified{});
			}
			catch (...)
			{
				return MU_LEAF_NEW_ERROR(gfx_error::not_specified{});
			}

			gfx_window_impl(std::shared_ptr<glfw_system> sys, int posX, int posY, int sizeX, int sizeY) : m_glfw_system(sys)
			{
				MU_LEAF_AUTO_THROW(new_window, create_window(posX, posY, sizeX, sizeY));
				glfwSetWindowUserPointer(new_window, this);
				m_window = new_window;
			}

			virtual ~gfx_window_impl()
			{
				if (m_imgui_context) [[likely]]
				{
					m_imgui_context.reset();
				}

				if (m_diligent_window) [[likely]]
				{
					m_diligent_window.reset();
				}

				if (m_renderer_globals) [[likely]]
				{
					m_renderer_globals.reset();
				}

				if (m_window) [[likely]]
				{
					destroy_window(m_window);
					m_window = nullptr;
				}
			}

			virtual auto wants_to_close() noexcept -> leaf::result<bool>
			try
			{
				return glfwWindowShouldClose(m_window) != 0;
			}
			catch (...)
			{
				return MU_LEAF_NEW_ERROR(gfx_error::not_specified{});
			}

			virtual auto show() noexcept -> leaf::result<void>
			try
			{
				glfwShowWindow(m_window);
				return {};
			}
			catch (...)
			{
				return MU_LEAF_NEW_ERROR(gfx_error::not_specified{});
			}

			auto init_imgui_resources() noexcept -> leaf::result<void>
			{
				m_imgui_context->make_current();

				// Setup backend capabilities flags
				ImGuiIO& io = ImGui::GetIO();
				io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;		  // We can honor GetMouseCursor() values (optional)
				io.BackendFlags |= ImGuiBackendFlags_HasSetMousePos;		  // We can honor io.WantSetMousePos requests (optional, rarely used)
				io.BackendFlags |= ImGuiBackendFlags_PlatformHasViewports;	  // We can create multi-viewports on the Platform side (optional)
				io.BackendFlags |= ImGuiBackendFlags_HasMouseHoveredViewport; // We can set io.MouseHoveredViewport correctly (optional, not easy)
				io.BackendPlatformName = "diligent_glfw";

				// Keyboard mapping. ImGui will use those indices to peek into the io.KeysDown[] array.
				io.KeyMap[ImGuiKey_Tab]			= GLFW_KEY_TAB;
				io.KeyMap[ImGuiKey_LeftArrow]	= GLFW_KEY_LEFT;
				io.KeyMap[ImGuiKey_RightArrow]	= GLFW_KEY_RIGHT;
				io.KeyMap[ImGuiKey_UpArrow]		= GLFW_KEY_UP;
				io.KeyMap[ImGuiKey_DownArrow]	= GLFW_KEY_DOWN;
				io.KeyMap[ImGuiKey_PageUp]		= GLFW_KEY_PAGE_UP;
				io.KeyMap[ImGuiKey_PageDown]	= GLFW_KEY_PAGE_DOWN;
				io.KeyMap[ImGuiKey_Home]		= GLFW_KEY_HOME;
				io.KeyMap[ImGuiKey_End]			= GLFW_KEY_END;
				io.KeyMap[ImGuiKey_Insert]		= GLFW_KEY_INSERT;
				io.KeyMap[ImGuiKey_Delete]		= GLFW_KEY_DELETE;
				io.KeyMap[ImGuiKey_Backspace]	= GLFW_KEY_BACKSPACE;
				io.KeyMap[ImGuiKey_Space]		= GLFW_KEY_SPACE;
				io.KeyMap[ImGuiKey_Enter]		= GLFW_KEY_ENTER;
				io.KeyMap[ImGuiKey_Escape]		= GLFW_KEY_ESCAPE;
				io.KeyMap[ImGuiKey_KeyPadEnter] = GLFW_KEY_KP_ENTER;
				io.KeyMap[ImGuiKey_A]			= GLFW_KEY_A;
				io.KeyMap[ImGuiKey_C]			= GLFW_KEY_C;
				io.KeyMap[ImGuiKey_V]			= GLFW_KEY_V;
				io.KeyMap[ImGuiKey_X]			= GLFW_KEY_X;
				io.KeyMap[ImGuiKey_Y]			= GLFW_KEY_Y;
				io.KeyMap[ImGuiKey_Z]			= GLFW_KEY_Z;

				io.ClipboardUserData = this;

				io.SetClipboardTextFn = [](void* user_data, const char* text)
				{
					auto self = reinterpret_cast<gfx_window_impl*>(user_data);
					glfwSetClipboardString(self->m_window, text);
				};

				io.GetClipboardTextFn = [](void* user_data)
				{
					auto self = reinterpret_cast<gfx_window_impl*>(user_data);
					return glfwGetClipboardString(self->m_window);
				};

				// Create mouse cursors
				// (By design, on X11 cursors are user configurable and some cursors may be missing. When a cursor doesn't exist,
				// GLFW will emit an error which will often be printed by the app, so we temporarily disable error reporting.
				// Missing cursors will return NULL and our _UpdateMouseCursor() function will use the Arrow cursor instead.)
				GLFWerrorfun prev_error_callback			= glfwSetErrorCallback(nullptr);
				g_MouseCursors[ImGuiMouseCursor_Arrow]		= glfwCreateStandardCursor(GLFW_ARROW_CURSOR);
				g_MouseCursors[ImGuiMouseCursor_TextInput]	= glfwCreateStandardCursor(GLFW_IBEAM_CURSOR);
				g_MouseCursors[ImGuiMouseCursor_ResizeNS]	= glfwCreateStandardCursor(GLFW_VRESIZE_CURSOR);
				g_MouseCursors[ImGuiMouseCursor_ResizeEW]	= glfwCreateStandardCursor(GLFW_HRESIZE_CURSOR);
				g_MouseCursors[ImGuiMouseCursor_Hand]		= glfwCreateStandardCursor(GLFW_HAND_CURSOR);
				g_MouseCursors[ImGuiMouseCursor_ResizeAll]	= glfwCreateStandardCursor(GLFW_RESIZE_ALL_CURSOR);
				g_MouseCursors[ImGuiMouseCursor_ResizeNESW] = glfwCreateStandardCursor(GLFW_RESIZE_NESW_CURSOR);
				g_MouseCursors[ImGuiMouseCursor_ResizeNWSE] = glfwCreateStandardCursor(GLFW_RESIZE_NWSE_CURSOR);
				g_MouseCursors[ImGuiMouseCursor_NotAllowed] = glfwCreateStandardCursor(GLFW_NOT_ALLOWED_CURSOR);

				glfwSetErrorCallback(prev_error_callback);

				// Chain GLFW callbacks: our callbacks will call the user's previously installed callbacks, if any.
				glfwSetMouseButtonCallback(
					m_window,
					[](GLFWwindow* window, int button, int action, int mods)
					{
						auto self = reinterpret_cast<gfx_window_impl*>(glfwGetWindowUserPointer(window));
						self->m_imgui_context->make_current();
						if (action == GLFW_PRESS && button >= 0 && button < self->g_MouseJustPressed.size())
						{
							self->g_MouseJustPressed[button] = true;
						}
					});

				glfwSetScrollCallback(
					m_window,
					[](GLFWwindow* window, double xoffset, double yoffset)
					{
						auto self = reinterpret_cast<gfx_window_impl*>(glfwGetWindowUserPointer(window));
						self->m_imgui_context->make_current();
						ImGuiIO& io = ImGui::GetIO();
						io.MouseWheelH += (float)xoffset;
						io.MouseWheel += (float)yoffset;
					});

				glfwSetKeyCallback(
					m_window,
					[](GLFWwindow* window, int key, int scancode, int action, int mods)
					{
						auto self = reinterpret_cast<gfx_window_impl*>(glfwGetWindowUserPointer(window));
						self->m_imgui_context->make_current();
						ImGuiIO& io = ImGui::GetIO();
						if (action == GLFW_PRESS)
						{
							io.KeysDown[key] = true;
						}

						if (action == GLFW_RELEASE)
						{
							io.KeysDown[key] = false;
						}

						// Modifiers are not reliable across systems
						io.KeyCtrl	= io.KeysDown[GLFW_KEY_LEFT_CONTROL] || io.KeysDown[GLFW_KEY_RIGHT_CONTROL];
						io.KeyShift = io.KeysDown[GLFW_KEY_LEFT_SHIFT] || io.KeysDown[GLFW_KEY_RIGHT_SHIFT];
						io.KeyAlt	= io.KeysDown[GLFW_KEY_LEFT_ALT] || io.KeysDown[GLFW_KEY_RIGHT_ALT];
#ifdef _WIN32
						io.KeySuper = false;
#else
						io.KeySuper = io.KeysDown[GLFW_KEY_LEFT_SUPER] || io.KeysDown[GLFW_KEY_RIGHT_SUPER];
#endif
					});

				glfwSetCharCallback(
					m_window,
					[](GLFWwindow* window, unsigned int c)
					{
						auto self = reinterpret_cast<gfx_window_impl*>(glfwGetWindowUserPointer(window));
						self->m_imgui_context->make_current();

						ImGuiIO& io = ImGui::GetIO();
						io.AddInputCharacter(c);
					});

				//glfwSetWindowSizeCallback(
				//	m_window,
				//	[](GLFWwindow* window, int, int)
				//	{
				//		auto self = reinterpret_cast<gfx_window_impl*>(glfwGetWindowUserPointer(window));
				//		self->m_imgui_context->make_current();
				//		if (ImGuiViewport* viewport = ImGui::FindViewportByPlatformHandle((void*)self->m_window))
				//		{
				//			viewport->PlatformRequestResize = true;
				//		}
				//	});

				ImGui_ImplGlfw_UpdateMonitors();

				glfwSetMonitorCallback(
					[](GLFWmonitor* mon, int)
					{
						auto self				   = reinterpret_cast<gfx_window_impl*>(glfwGetMonitorUserPointer(mon));
						self->g_WantUpdateMonitors = true;
					});

				glfwSetWindowContentScaleCallback(
					m_window,
					[](GLFWwindow* window, float, float)
					{
						auto self = reinterpret_cast<gfx_window_impl*>(glfwGetWindowUserPointer(window));
						self->m_imgui_context->make_current();

						self->g_WantUpdateMonitors = true;
					});

				// if (primary)
				{
					ImGuiViewport* main_viewport	= ImGui::GetMainViewport();
					main_viewport->PlatformUserData = this;
					main_viewport->PlatformHandle	= (void*)m_window;

					ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
					// platform_io.Platform_CreateWindow		= ImGui_ImplGlfw_CreateWindow;
					platform_io.Platform_DestroyWindow = [](ImGuiViewport* viewport)
					{
						//						if (auto self = reinterpret_cast<gfx_window_impl*>(viewport->PlatformUserData))
						//						{
						//							if (self->WindowOwned)
						//							{
						//								glfwDestroyWindow(data->Window);
						//							}
						//							data->Window = NULL;
						//							IM_DELETE(data);
						//						}
						viewport->PlatformUserData = viewport->PlatformHandle = NULL;
					};
					// platform_io.Platform_ShowWindow			= ImGui_ImplGlfw_ShowWindow;
					// platform_io.Platform_SetWindowPos		= ImGui_ImplGlfw_SetWindowPos;
					// platform_io.Platform_GetWindowPos		= ImGui_ImplGlfw_GetWindowPos;
					// platform_io.Platform_SetWindowSize		= ImGui_ImplGlfw_SetWindowSize;
					// platform_io.Platform_GetWindowSize		= ImGui_ImplGlfw_GetWindowSize;
					// platform_io.Platform_SetWindowFocus		= ImGui_ImplGlfw_SetWindowFocus;
					// platform_io.Platform_GetWindowFocus		= ImGui_ImplGlfw_GetWindowFocus;
					// platform_io.Platform_GetWindowMinimized = ImGui_ImplGlfw_GetWindowMinimized;
					// platform_io.Platform_SetWindowTitle		= ImGui_ImplGlfw_SetWindowTitle;
					// platform_io.Platform_RenderWindow		= ImGui_ImplGlfw_RenderWindow;
					// platform_io.Platform_SwapBuffers		= ImGui_ImplGlfw_SwapBuffers;
					// platform_io.Platform_SetWindowAlpha		= ImGui_ImplGlfw_SetWindowAlpha;
					//#if HAS_WIN32_IME
					// platform_io.Platform_SetImeInputPos = ImGui_ImplWin32_SetImeInputPos;
					//#endif
				}

				return {};
			}

			void ImGui_ImplGlfw_UpdateMousePosAndButtons()
			{
				// Update buttons
				ImGuiIO& io = ImGui::GetIO();
				for (int i = 0; i < IM_ARRAYSIZE(io.MouseDown); i++)
				{
					// If a mouse press event came, always pass it as "mouse held this frame", so we don't miss click-release events that are shorter than 1 frame.
					io.MouseDown[i]		  = g_MouseJustPressed[i] || glfwGetMouseButton(m_window, i) != 0;
					g_MouseJustPressed[i] = false;
				}

				// Update mouse position
				const ImVec2 mouse_pos_backup = io.MousePos;
				io.MousePos					  = ImVec2(-FLT_MAX, -FLT_MAX);
				io.MouseHoveredViewport		  = 0;
				ImGuiPlatformIO& platform_io  = ImGui::GetPlatformIO();
				for (int n = 0; n < platform_io.Viewports.Size; n++)
				{
					ImGuiViewport* viewport = platform_io.Viewports[n];
					auto		   self		= reinterpret_cast<gfx_window_impl*>(viewport->PlatformUserData);
					IM_ASSERT(self->m_window != NULL);
					const bool focused = glfwGetWindowAttrib(self->m_window, GLFW_FOCUSED) != 0;
					if (focused)
					{
						if (io.WantSetMousePos)
						{
							glfwSetCursorPos(self->m_window, (double)(mouse_pos_backup.x - viewport->Pos.x), (double)(mouse_pos_backup.y - viewport->Pos.y));
						}
						else
						{
							double mouse_x, mouse_y;
							glfwGetCursorPos(self->m_window, &mouse_x, &mouse_y);
							if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
							{
								// Multi-viewport mode: mouse position in OS absolute coordinates (io.MousePos is (0,0) when the mouse is on the upper-left of the primary monitor)
								int window_x, window_y;
								glfwGetWindowPos(self->m_window, &window_x, &window_y);
								io.MousePos = ImVec2((float)mouse_x + window_x, (float)mouse_y + window_y);
							}
							else
							{
								// Single viewport mode: mouse position in client window coordinates (io.MousePos is (0,0) when the mouse is on the upper-left corner of the app
								// window)
								io.MousePos = ImVec2((float)mouse_x, (float)mouse_y);
							}
						}
						for (int i = 0; i < IM_ARRAYSIZE(io.MouseDown); i++)
							io.MouseDown[i] |= glfwGetMouseButton(self->m_window, i) != 0;
					}

					// (Optional) When using multiple viewports: set io.MouseHoveredViewport to the viewport the OS mouse cursor is hovering.
					// Important: this information is not easy to provide and many high-level windowing library won't be able to provide it correctly, because
					// - This is _ignoring_ viewports with the ImGuiViewportFlags_NoInputs flag (pass-through windows).
					// - This is _regardless_ of whether another viewport is focused or being dragged from.
					// If ImGuiBackendFlags_HasMouseHoveredViewport is not set by the backend, imgui will ignore this field and infer the information by relying on the
					// rectangles and last focused time of every viewports it knows about. It will be unaware of other windows that may be sitting between or over your windows.
					// [GLFW] FIXME: This is currently only correct on Win32. See what we do below with the WM_NCHITTEST, missing an equivalent for other systems.
					// See https://github.com/glfw/glfw/issues/1236 if you want to help in making this a GLFW feature.
					const bool window_no_input = (viewport->Flags & ImGuiViewportFlags_NoInputs) != 0;
					glfwSetWindowAttrib(self->m_window, GLFW_MOUSE_PASSTHROUGH, window_no_input);
					if (glfwGetWindowAttrib(self->m_window, GLFW_HOVERED) && !window_no_input)
						io.MouseHoveredViewport = viewport->ID;
				}
			}

			void ImGui_ImplGlfw_UpdateMouseCursor()
			{
				ImGuiIO& io = ImGui::GetIO();
				if ((io.ConfigFlags & ImGuiConfigFlags_NoMouseCursorChange) || glfwGetInputMode(m_window, GLFW_CURSOR) == GLFW_CURSOR_DISABLED)
					return;

				ImGuiMouseCursor imgui_cursor = ImGui::GetMouseCursor();
				ImGuiPlatformIO& platform_io  = ImGui::GetPlatformIO();
				for (int n = 0; n < platform_io.Viewports.Size; n++)
				{
					auto self = reinterpret_cast<gfx_window_impl*>(platform_io.Viewports[n]->PlatformUserData);
					if (imgui_cursor == ImGuiMouseCursor_None || io.MouseDrawCursor)
					{
						// Hide OS mouse cursor if imgui is drawing it or if it wants no cursor
						glfwSetInputMode(self->m_window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
					}
					else
					{
						// Show OS mouse cursor
						// FIXME-PLATFORM: Unfocused windows seems to fail changing the mouse cursor with GLFW 3.2, but 3.3 works here.
						glfwSetCursor(self->m_window, g_MouseCursors[imgui_cursor] ? g_MouseCursors[imgui_cursor] : g_MouseCursors[ImGuiMouseCursor_Arrow]);
						glfwSetInputMode(self->m_window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
					}
				}
			}

			void ImGui_ImplGlfw_UpdateGamepads()
			{
				ImGuiIO& io = ImGui::GetIO();
				memset(io.NavInputs, 0, sizeof(io.NavInputs));
				if ((io.ConfigFlags & ImGuiConfigFlags_NavEnableGamepad) == 0)
					return;

// Update gamepad inputs
#define MAP_BUTTON(NAV_NO, BUTTON_NO)                                                                                                                                              \
	{                                                                                                                                                                              \
		if (buttons_count > BUTTON_NO && buttons[BUTTON_NO] == GLFW_PRESS)                                                                                                         \
			io.NavInputs[NAV_NO] = 1.0f;                                                                                                                                           \
	}
#define MAP_ANALOG(NAV_NO, AXIS_NO, V0, V1)                                                                                                                                        \
	{                                                                                                                                                                              \
		float v = (axes_count > AXIS_NO) ? axes[AXIS_NO] : V0;                                                                                                                     \
		v		= (v - V0) / (V1 - V0);                                                                                                                                            \
		if (v > 1.0f)                                                                                                                                                              \
			v = 1.0f;                                                                                                                                                              \
		if (io.NavInputs[NAV_NO] < v)                                                                                                                                              \
			io.NavInputs[NAV_NO] = v;                                                                                                                                              \
	}
				int					 axes_count = 0, buttons_count = 0;
				const float*		 axes	 = glfwGetJoystickAxes(GLFW_JOYSTICK_1, &axes_count);
				const unsigned char* buttons = glfwGetJoystickButtons(GLFW_JOYSTICK_1, &buttons_count);
				MAP_BUTTON(ImGuiNavInput_Activate, 0);	 // Cross / A
				MAP_BUTTON(ImGuiNavInput_Cancel, 1);	 // Circle / B
				MAP_BUTTON(ImGuiNavInput_Menu, 2);		 // Square / X
				MAP_BUTTON(ImGuiNavInput_Input, 3);		 // Triangle / Y
				MAP_BUTTON(ImGuiNavInput_DpadLeft, 13);	 // D-Pad Left
				MAP_BUTTON(ImGuiNavInput_DpadRight, 11); // D-Pad Right
				MAP_BUTTON(ImGuiNavInput_DpadUp, 10);	 // D-Pad Up
				MAP_BUTTON(ImGuiNavInput_DpadDown, 12);	 // D-Pad Down
				MAP_BUTTON(ImGuiNavInput_FocusPrev, 4);	 // L1 / LB
				MAP_BUTTON(ImGuiNavInput_FocusNext, 5);	 // R1 / RB
				MAP_BUTTON(ImGuiNavInput_TweakSlow, 4);	 // L1 / LB
				MAP_BUTTON(ImGuiNavInput_TweakFast, 5);	 // R1 / RB
				MAP_ANALOG(ImGuiNavInput_LStickLeft, 0, -0.3f, -0.9f);
				MAP_ANALOG(ImGuiNavInput_LStickRight, 0, +0.3f, +0.9f);
				MAP_ANALOG(ImGuiNavInput_LStickUp, 1, +0.3f, +0.9f);
				MAP_ANALOG(ImGuiNavInput_LStickDown, 1, -0.3f, -0.9f);
#undef MAP_BUTTON
#undef MAP_ANALOG
				if (axes_count > 0 && buttons_count > 0)
					io.BackendFlags |= ImGuiBackendFlags_HasGamepad;
				else
					io.BackendFlags &= ~ImGuiBackendFlags_HasGamepad;
			}

			void ImGui_ImplGlfw_UpdateMonitors()
			{
				ImGuiPlatformIO& platform_io	= ImGui::GetPlatformIO();
				int				 monitors_count = 0;
				GLFWmonitor**	 glfw_monitors	= glfwGetMonitors(&monitors_count);
				platform_io.Monitors.resize(0);
				for (int n = 0; n < monitors_count; n++)
				{
					glfwSetMonitorUserPointer(glfw_monitors[n], this);

					ImGuiPlatformMonitor monitor;
					int					 x, y;
					glfwGetMonitorPos(glfw_monitors[n], &x, &y);
					const GLFWvidmode* vid_mode = glfwGetVideoMode(glfw_monitors[n]);
					monitor.MainPos = monitor.WorkPos = ImVec2((float)x, (float)y);
					monitor.MainSize = monitor.WorkSize = ImVec2((float)vid_mode->width, (float)vid_mode->height);

					int w, h;
					glfwGetMonitorWorkarea(glfw_monitors[n], &x, &y, &w, &h);
					monitor.WorkPos	 = ImVec2((float)x, (float)y);
					monitor.WorkSize = ImVec2((float)w, (float)h);

					// Warning: the validity of monitor DPI information on Windows depends on the application DPI awareness settings, which generally needs to be set in the
					// manifest or at runtime.
					float x_scale, y_scale;
					glfwGetMonitorContentScale(glfw_monitors[n], &x_scale, &y_scale);
					monitor.DpiScale = y_scale;

					platform_io.Monitors.push_back(monitor);
				}
				g_WantUpdateMonitors = false;
			}

			void ImGui_ImplGlfw_NewFrame()
			{
				ImGuiIO& io = ImGui::GetIO();
				IM_ASSERT(io.Fonts->IsBuilt() && "Font atlas not built!");

				// Setup display size (every frame to accommodate for window resizing)
				int w, h;
				int display_w, display_h;
				glfwGetWindowSize(m_window, &w, &h);
				glfwGetFramebufferSize(m_window, &display_w, &display_h);
				io.DisplaySize = ImVec2((float)w, (float)h);

				if (w > 0 && h > 0)
					io.DisplayFramebufferScale = ImVec2((float)display_w / w, (float)display_h / h);

				if (g_WantUpdateMonitors)
					ImGui_ImplGlfw_UpdateMonitors();

				// Setup time step
				double current_time = glfwGetTime();
				io.DeltaTime		= /*g_Time > 0.0 ? (float)(current_time - g_Time) :*/ (float)(1.0f / 60.0f);
				// g_Time				= current_time;

				ImGui_ImplGlfw_UpdateMousePosAndButtons();
				ImGui_ImplGlfw_UpdateMouseCursor();

				// Update game controllers (if enabled and available)
				ImGui_ImplGlfw_UpdateGamepads();
			}

			auto init_resources() noexcept -> mu::leaf::result<void>
			{
				if (!m_renderer_globals) [[unlikely]]
				{
					try
					{
						m_renderer_globals = std::make_shared<diligent_globals>();
					}
					catch (...)
					{
						return MU_LEAF_NEW_ERROR(mu::gfx_error::not_specified{});
					}
				}

				if (!m_diligent_window) [[unlikely]]
				{
					try
					{
						// TODO: do a better job figuring out scale, default monitor isn't necessarily right
						auto mon = glfwGetWindowMonitor(m_window);
						if (!mon)
						{
							mon = glfwGetPrimaryMonitor();
						}
						m_diligent_window = std::make_shared<diligent_window>(Diligent::Win32NativeWindow{glfwGetWin32Window(m_window)}, m_renderer_globals);
						m_imgui_context	  = std::make_shared<imgui_context>(m_window, m_diligent_window);
					}
					catch (...)
					{
						return MU_LEAF_NEW_ERROR(mu::gfx_error::not_specified{});
					}

					MU_LEAF_CHECK(init_imgui_resources());
				}

				return {};
			}

			virtual auto begin_frame() noexcept -> mu::leaf::result<void>
			{
				MU_LEAF_CHECK(init_resources());

				if (m_diligent_window) [[likely]]
				{
					try
					{
						glfwGetFramebufferSize(m_window, &m_display_size[0], &m_display_size[1]);
					}
					catch (...)
					{
						return MU_LEAF_NEW_ERROR(mu::gfx_error::not_specified{});
					}

					MU_LEAF_CHECK(m_diligent_window->create_resources(m_display_size[0], m_display_size[1]));
					MU_LEAF_CHECK(m_diligent_window->clear());
					return {};
				}
				else
				{
					return MU_LEAF_NEW_ERROR(mu::gfx_error::not_specified{});
				}
			}

			auto present() noexcept -> mu::leaf::result<void>
			{
				MU_LEAF_CHECK(m_diligent_window->present());
				return {};
			}

			virtual auto begin_imgui() noexcept -> mu::leaf::result<void>
			{
				try
				{
					ImGui::SetCurrentContext(m_imgui_context->m_imgui_context.get());

					ImGui_ImplGlfw_NewFrame();

					auto m_dpi_scale = get_dpi_scale_for_hwnd(glfwGetWin32Window(m_window));

					m_imgui_context->m_imgui_renderer
						->new_frame(m_display_size[0], m_display_size[1], Diligent::SURFACE_TRANSFORM::SURFACE_TRANSFORM_OPTIMAL, m_dpi_scale);

					ImGui::NewFrame();

					return {};
				}
				catch (...)
				{
					return MU_LEAF_NEW_ERROR(mu::gfx_error::not_specified{});
				}
			}

			virtual auto end_imgui() noexcept -> mu::leaf::result<void>
			{
				try
				{
					ImGui::SetCurrentContext(m_imgui_context->m_imgui_context.get());
					ImGui::Render();
					m_imgui_context->m_imgui_renderer->render_draw_data(m_diligent_window->m_immediate_context, ImGui::GetDrawData());
					ImGui::EndFrame();
					return {};
				}
				catch (...)
				{
					return MU_LEAF_NEW_ERROR(mu::gfx_error::not_specified{});
				}
			}

			virtual auto end_frame() noexcept -> mu::leaf::result<void>
			try
			{
				return {};
			}
			catch (...)
			{
				return MU_LEAF_NEW_ERROR(gfx_error::not_specified{});
			}
		};
	} // namespace details
} // namespace mu

namespace mu
{
	namespace details
	{
		struct gfx_impl : public gfx_interface
		{
			std::weak_ptr<glfw_system> m_glfw_system;
			auto					   get_system_ref() noexcept -> std::shared_ptr<glfw_system>
			{
				auto system_ref = (m_glfw_system.expired()) ? std::make_shared<glfw_system>() : m_glfw_system.lock();
				if (m_glfw_system.expired()) [[unlikely]]
				{
					m_glfw_system = system_ref;
				}
				return system_ref;
			}

			gfx_impl() = default;

			virtual ~gfx_impl() = default;

			std::vector<std::weak_ptr<gfx_window_impl>> m_windows;

			virtual auto open_window(int posX, int posY, int sizeX, int sizeY) noexcept -> mu::leaf::result<std::shared_ptr<gfx_window>>
			try
			{
				auto system_ref = get_system_ref();
				auto new_window = std::make_shared<gfx_window_impl>(system_ref, posX, posY, sizeX, sizeY);
				m_windows.push_back(std::static_pointer_cast<gfx_window_impl>(new_window));
				return new_window;
			}
			catch (...)
			{
				return MU_LEAF_NEW_ERROR(mu::gfx_error::not_specified{});
			}

			virtual auto pump() noexcept -> mu::leaf::result<void>
			try
			{
				if (!m_glfw_system.expired()) [[likely]]
				{
					glfwPollEvents();
					return {};
				}
				else
				{
					return MU_LEAF_NEW_ERROR(gfx_error::not_specified{});
				}
			}
			catch (...)
			{
				return MU_LEAF_NEW_ERROR(gfx_error::not_specified{});
			}

			virtual auto present() noexcept -> mu::leaf::result<void>
			{
				for (auto itor = m_windows.begin(); itor != m_windows.end();)
				{
					if (itor->expired()) [[unlikely]]
					{
						itor = m_windows.erase(itor);
					}
					else
					{
						MU_LEAF_CHECK(itor->lock()->present());
						++itor;
					}
				}
				return {};
			}
		};
	} // namespace details
} // namespace mu

MU_DEFINE_VIRTUAL_SINGLETON(mu::details::gfx_interface, mu::details::gfx_impl);
MU_EXPORT_SINGLETON(mu::gfx);