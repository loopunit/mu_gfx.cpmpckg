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

#include <unordered_map>

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
		struct gfx_application_state
		{
			gfx_application_state() : m_imgui_lib_context(ImGui::CreateContext(), ImGui::DestroyContext) { }

			std::shared_ptr<ImGuiContext>					m_imgui_lib_context;
			std::array<GLFWcursor*, ImGuiMouseCursor_COUNT> m_mouse_cursors;
			std::array<bool, ImGuiMouseButton_COUNT>		m_mouse_just_pressed;
			bool											m_want_update_monitors{false};
			time::moment									m_timer;
			bool											m_timer_ready = false;

			auto make_current() noexcept -> leaf::result<void>
			try
			{
				ImGui::SetCurrentContext(m_imgui_lib_context.get());
				return {};
			}
			catch (...)
			{
				return MU_LEAF_NEW_ERROR(gfx_error::not_specified{});
			}
		};

		struct gfx_child_window
		{
			std::shared_ptr<gfx_application_state>	  m_application_state;
			std::shared_ptr<GLFWwindow>				  m_window;
			std::shared_ptr<diligent_child_window>	  m_diligent_window;
			std::shared_ptr<Diligent::imgui_renderer> m_imgui_renderer;

			std::array<int, 2> m_display_size{0, 0};
			float			   m_dpi_scale{1.0f};

			gfx_child_window(std::shared_ptr<gfx_application_state> application_state, ImGuiViewport* viewport, int posX, int posY, int sizeX, int sizeY) : m_application_state(application_state)
			{
				MU_LEAF_AUTO_THROW(new_window, create_window(viewport, posX, posY, sizeX, sizeY));

				auto wnd = std::shared_ptr<GLFWwindow>(
					new_window,
					[](GLFWwindow* w) -> void
					{
						try
						{
							glfwDestroyWindow(w);
						}
						catch (...)
						{
						}
					});

				glfwSetMouseButtonCallback(
					wnd.get(),
					[](GLFWwindow* window, int button, int action, int mods) -> void
					{
						auto self = reinterpret_cast<gfx_child_window*>(glfwGetWindowUserPointer(window));
						MU_LEAF_RETHROW(self->m_application_state->make_current());
						if (action == GLFW_PRESS && button >= 0 && button < self->m_application_state->m_mouse_just_pressed.size())
						{
							self->m_application_state->m_mouse_just_pressed[button] = true;
						}
					});

				glfwSetScrollCallback(
					wnd.get(),
					[](GLFWwindow* window, double xoffset, double yoffset) -> void
					{
						auto self = reinterpret_cast<gfx_child_window*>(glfwGetWindowUserPointer(window));
						MU_LEAF_RETHROW(self->m_application_state->make_current());
						ImGuiIO& io = ImGui::GetIO();
						io.MouseWheelH += (float)xoffset;
						io.MouseWheel += (float)yoffset;
					});

				glfwSetKeyCallback(
					wnd.get(),
					[](GLFWwindow* window, int key, int scancode, int action, int mods) -> void
					{
						auto self = reinterpret_cast<gfx_child_window*>(glfwGetWindowUserPointer(window));
						MU_LEAF_RETHROW(self->m_application_state->make_current());
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
					wnd.get(),
					[](GLFWwindow* window, unsigned int c) -> void
					{
						auto self = reinterpret_cast<gfx_child_window*>(glfwGetWindowUserPointer(window));
						MU_LEAF_RETHROW(self->m_application_state->make_current());
						ImGuiIO& io = ImGui::GetIO();
						io.AddInputCharacter(c);
					});

				glfwSetWindowContentScaleCallback(
					wnd.get(),
					[](GLFWwindow* window, float, float) -> void
					{
						auto self = reinterpret_cast<gfx_child_window*>(glfwGetWindowUserPointer(window));
						MU_LEAF_RETHROW(self->m_application_state->make_current());
					});

				m_window = std::move(wnd);
			}

			virtual ~gfx_child_window()
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
					m_diligent_window.reset();
				}
				catch (...)
				{
					MU_LEAF_LOG_ERROR(mu::gfx_error::not_specified{});
				}

				try
				{
					m_window.reset();
				}
				catch (...)
				{
					MU_LEAF_LOG_ERROR(mu::gfx_error::not_specified{});
				}
			}

			[[nodiscard]] auto create_window(ImGuiViewport* viewport, int posX, int posY, int sizeX, int sizeY) noexcept -> leaf::result<GLFWwindow*>
			try
			{
				glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
				glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_TRUE);
				glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
				glfwWindowHint(GLFW_FOCUSED, GLFW_FALSE);
				glfwWindowHint(GLFW_FOCUS_ON_SHOW, GLFW_FALSE);
				glfwWindowHint(GLFW_DECORATED, (viewport->Flags & ImGuiViewportFlags_NoDecoration) ? false : true);
				glfwWindowHint(GLFW_FLOATING, (viewport->Flags & ImGuiViewportFlags_TopMost) ? true : false);

				if (auto wnd = glfwCreateWindow(sizeX, sizeY, "", NULL, NULL); wnd != nullptr) [[likely]]
				{
					try
					{
						glfwSetWindowPos(wnd, posX, posY);
						glfwGetFramebufferSize(wnd, &m_display_size[0], &m_display_size[1]);
						glfwSetWindowUserPointer(wnd, this);
						m_dpi_scale = get_dpi_scale_for_hwnd(glfwGetWin32Window(wnd));
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

			[[nodiscard]] auto wants_to_close() noexcept -> leaf::result<bool>
			try
			{
				return glfwWindowShouldClose(m_window.get()) != 0;
			}
			catch (...)
			{
				return MU_LEAF_NEW_ERROR(gfx_error::not_specified{});
			}

			[[nodiscard]] auto init_resources(std::shared_ptr<diligent_globals> globals, std::shared_ptr<Diligent::imgui_shared_resources> shared_resources) noexcept
				-> mu::leaf::result<void>
			{
				if (!m_diligent_window) [[unlikely]]
				{
					try
					{
						m_diligent_window = std::make_shared<diligent_child_window>(Diligent::Win32NativeWindow{glfwGetWin32Window(m_window.get())}, globals);

						const auto& swapchain_desc = m_diligent_window->m_swap_chain->GetDesc();
						m_imgui_renderer		   = std::make_shared<Diligent::imgui_renderer>(shared_resources, 1024 * 1024, 1024 * 1024, m_dpi_scale);
					}
					catch (...)
					{
						return MU_LEAF_NEW_ERROR(mu::gfx_error::not_specified{});
					}
				}

				return {};
			}

			[[nodiscard]] auto begin_imgui() noexcept -> mu::leaf::result<void>
			{
				m_dpi_scale = get_dpi_scale_for_hwnd(glfwGetWin32Window(m_window.get()));

				MU_LEAF_CHECK(m_imgui_renderer->new_frame(m_display_size[0], m_display_size[1], Diligent::SURFACE_TRANSFORM::SURFACE_TRANSFORM_OPTIMAL, m_dpi_scale));
				return {};
			}

			[[nodiscard]] auto begin_frame(std::shared_ptr<diligent_globals> globals, std::shared_ptr<Diligent::imgui_shared_resources> shared_resources) noexcept
				-> mu::leaf::result<void>
			{
				MU_LEAF_CHECK(init_resources(globals, shared_resources));

				if (m_diligent_window) [[likely]]
				{
					try
					{
						glfwGetFramebufferSize(m_window.get(), &m_display_size[0], &m_display_size[1]);
					}
					catch (...)
					{
						return MU_LEAF_NEW_ERROR(mu::gfx_error::not_specified{});
					}

					MU_LEAF_CHECK(m_diligent_window->create_resources(m_display_size[0], m_display_size[1]));

					return {};
				}
				else
				{
					return MU_LEAF_NEW_ERROR(mu::gfx_error::not_specified{});
				}
			}

			[[nodiscard]] auto render(Diligent::IDeviceContext* ctx, ImDrawData* draw_data) noexcept -> mu::leaf::result<void>
			{
				if (m_diligent_window)
				{
					MU_LEAF_CHECK(m_diligent_window->clear());
					MU_LEAF_CHECK(m_imgui_renderer->render_draw_data(ctx, draw_data));
				}
				return {};
			}

			[[nodiscard]] auto present() noexcept -> mu::leaf::result<void>
			{
				if (m_diligent_window && m_diligent_window->m_swap_chain)
				{
					MU_LEAF_CHECK(m_diligent_window->present());
				}
				return {};
			}
		};

		struct gfx_window_impl : public gfx_window
		{
			std::shared_ptr<glfw_system> m_glfw_system;
			std::shared_ptr<GLFWwindow>	 m_window;

			std::shared_ptr<diligent_window>		  m_diligent_window;
			std::shared_ptr<diligent_globals>		  m_renderer_globals;
			std::shared_ptr<Diligent::imgui_renderer> m_imgui_renderer;
			std::shared_ptr<gfx_application_state>	  m_application_state;

			std::array<int, 2> m_display_size{0, 0};
			float			   m_dpi_scale{1.0f};

			gfx_window_impl(std::shared_ptr<glfw_system> sys, int posX, int posY, int sizeX, int sizeY)
				: m_glfw_system(sys)
				, m_application_state(std::make_shared<gfx_application_state>())
			{
				MU_LEAF_AUTO_THROW(new_window, create_window(posX, posY, sizeX, sizeY));
				glfwSetWindowUserPointer(new_window, this);
				m_window = std::shared_ptr<GLFWwindow>(
					new_window,
					[](GLFWwindow* w) -> void
					{
						try
						{
							glfwDestroyWindow(w);
						}
						catch (...)
						{
						}
					});
			}

			virtual ~gfx_window_impl()
			{
				m_application_state->make_current();
				ImGuiViewport* main_viewport	= ImGui::GetMainViewport();
				main_viewport->PlatformUserData = nullptr;
				main_viewport->PlatformHandle	= nullptr;

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
					m_diligent_window.reset();
				}
				catch (...)
				{
					MU_LEAF_LOG_ERROR(mu::gfx_error::not_specified{});
				}

				if (m_renderer_globals) [[likely]]
				{
					m_renderer_globals.reset();
				}

				if (m_window) [[likely]]
				{
					m_window.reset();
					m_window = nullptr;
				}
			}

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
						m_dpi_scale = get_dpi_scale_for_hwnd(glfwGetWin32Window(wnd));
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

			auto wants_to_close() noexcept -> leaf::result<bool>
			try
			{
				return glfwWindowShouldClose(m_window.get()) != 0;
			}
			catch (...)
			{
				return MU_LEAF_NEW_ERROR(gfx_error::not_specified{});
			}

			auto show() noexcept -> leaf::result<void>
			try
			{
				glfwShowWindow(m_window.get());
				return {};
			}
			catch (...)
			{
				return MU_LEAF_NEW_ERROR(gfx_error::not_specified{});
			}

			auto init_glfw_resources() noexcept -> leaf::result<void>
			{
				try
				{
					glfwSetMouseButtonCallback(
						m_window.get(),
						[](GLFWwindow* window, int button, int action, int mods) -> void
						{
							auto self = reinterpret_cast<gfx_window_impl*>(glfwGetWindowUserPointer(window));
							MU_LEAF_RETHROW(self->m_application_state->make_current());
							if (action == GLFW_PRESS && button >= 0 && button < self->m_application_state->m_mouse_just_pressed.size())
							{
								self->m_application_state->m_mouse_just_pressed[button] = true;
							}
						});

					glfwSetScrollCallback(
						m_window.get(),
						[](GLFWwindow* window, double xoffset, double yoffset) -> void
						{
							auto self = reinterpret_cast<gfx_window_impl*>(glfwGetWindowUserPointer(window));
							MU_LEAF_RETHROW(self->m_application_state->make_current())
							ImGuiIO& io = ImGui::GetIO();
							io.MouseWheelH += (float)xoffset;
							io.MouseWheel += (float)yoffset;
						});

					glfwSetKeyCallback(
						m_window.get(),
						[](GLFWwindow* window, int key, int scancode, int action, int mods) -> void
						{
							auto self = reinterpret_cast<gfx_window_impl*>(glfwGetWindowUserPointer(window));
							MU_LEAF_RETHROW(self->m_application_state->make_current())
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
						m_window.get(),
						[](GLFWwindow* window, unsigned int c) -> void
						{
							auto self = reinterpret_cast<gfx_window_impl*>(glfwGetWindowUserPointer(window));
							MU_LEAF_RETHROW(self->m_application_state->make_current())
							ImGuiIO& io = ImGui::GetIO();
							io.AddInputCharacter(c);
						});

					glfwSetWindowContentScaleCallback(
						m_window.get(),
						[](GLFWwindow* window, float, float) -> void
						{
							auto self = reinterpret_cast<gfx_window_impl*>(glfwGetWindowUserPointer(window));
							MU_LEAF_RETHROW(self->m_application_state->make_current());
						});
				}
				catch (...)
				{
					return MU_LEAF_NEW_ERROR(mu::gfx_error::not_specified{});
				}

				return {};
			}

			[[nodiscard]] auto init_window() noexcept -> leaf::result<void>
			{
				MU_LEAF_CHECK(m_application_state->make_current());

				try
				{
					// Setup backend capabilities flags
					ImGuiIO& io = ImGui::GetIO();
					io.BackendFlags |= ImGuiBackendFlags_RendererHasViewports;	  // We can create multi-viewports on the Renderer side (optional)
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

					io.SetClipboardTextFn = [](void* user_data, const char* text) -> void
					{
						auto self = reinterpret_cast<gfx_window_impl*>(user_data);
						glfwSetClipboardString(self->m_window.get(), text);
					};

					io.GetClipboardTextFn = [](void* user_data) -> const char*
					{
						auto self = reinterpret_cast<gfx_window_impl*>(user_data);
						return glfwGetClipboardString(self->m_window.get());
					};

					// Create mouse cursors
					// (By design, on X11 cursors are user configurable and some cursors may be missing. When a cursor doesn't exist,
					// GLFW will emit an error which will often be printed by the app, so we temporarily disable error reporting.
					// Missing cursors will return NULL and our _UpdateMouseCursor() function will use the Arrow cursor instead.)
					GLFWerrorfun prev_error_callback								  = glfwSetErrorCallback(nullptr);
					m_application_state->m_mouse_cursors[ImGuiMouseCursor_Arrow]	  = glfwCreateStandardCursor(GLFW_ARROW_CURSOR);
					m_application_state->m_mouse_cursors[ImGuiMouseCursor_TextInput]  = glfwCreateStandardCursor(GLFW_IBEAM_CURSOR);
					m_application_state->m_mouse_cursors[ImGuiMouseCursor_ResizeNS]	  = glfwCreateStandardCursor(GLFW_VRESIZE_CURSOR);
					m_application_state->m_mouse_cursors[ImGuiMouseCursor_ResizeEW]	  = glfwCreateStandardCursor(GLFW_HRESIZE_CURSOR);
					m_application_state->m_mouse_cursors[ImGuiMouseCursor_Hand]		  = glfwCreateStandardCursor(GLFW_HAND_CURSOR);
					m_application_state->m_mouse_cursors[ImGuiMouseCursor_ResizeAll]  = glfwCreateStandardCursor(GLFW_RESIZE_ALL_CURSOR);
					m_application_state->m_mouse_cursors[ImGuiMouseCursor_ResizeNESW] = glfwCreateStandardCursor(GLFW_RESIZE_NESW_CURSOR);
					m_application_state->m_mouse_cursors[ImGuiMouseCursor_ResizeNWSE] = glfwCreateStandardCursor(GLFW_RESIZE_NWSE_CURSOR);
					m_application_state->m_mouse_cursors[ImGuiMouseCursor_NotAllowed] = glfwCreateStandardCursor(GLFW_NOT_ALLOWED_CURSOR);
					glfwSetErrorCallback(prev_error_callback);

					MU_LEAF_CHECK(init_glfw_resources());

					ImGuiViewport* main_viewport	= ImGui::GetMainViewport();
					main_viewport->PlatformUserData = this;
					main_viewport->PlatformHandle	= m_window.get();

					ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();

					platform_io.Platform_CreateWindow = [](ImGuiViewport* viewport) -> void
					{
						if (ImGuiViewport* main_viewport = ImGui::GetMainViewport(); main_viewport != viewport)
						{
							auto			  main_window = static_cast<gfx_window_impl*>(main_viewport->PlatformUserData);
							gfx_child_window* cw =
								new gfx_child_window(main_window->m_application_state, viewport, (int)viewport->Pos.x, (int)viewport->Pos.y, (int)viewport->Size.x, (int)viewport->Size.y);
							viewport->PlatformUserData = cw;
							viewport->PlatformHandle   = cw->m_window.get();
							// viewport->RendererUserData
							// viewport->PlatformHandleRaw
							// viewport->PlatformRequestMove
							// viewport->PlatformRequestResize
							// viewport->PlatformRequestClose
							// viewport->PlatformUserData = this;
							// viewport->PlatformHandle	= m_window.get();
						}
					};

					platform_io.Platform_DestroyWindow = [](ImGuiViewport* viewport) -> void
					{
						if (ImGuiViewport* main_viewport = ImGui::GetMainViewport(); main_viewport != viewport)
						{
							auto cw					   = static_cast<gfx_child_window*>(viewport->PlatformUserData);
							viewport->PlatformUserData = nullptr;
							viewport->PlatformHandle   = nullptr;
							delete cw;
							// viewport->PlatformUserData = this;
							// viewport->PlatformHandle	= m_window.get();
						}
					};

					platform_io.Platform_ShowWindow = [](ImGuiViewport* viewport) -> void
					{
						GLFWwindow* wnd = static_cast<GLFWwindow*>(viewport->PlatformHandle);
						glfwShowWindow(wnd);
						//#if defined(_WIN32)
						// HWND hwnd = (HWND)viewport->PlatformHandleRaw;
						// if (viewport->Flags & ImGuiViewportFlags_NoTaskBarIcon)
						//{
						// LONG ex_style = ::GetWindowLong(hwnd, GWL_EXSTYLE);
						// ex_style &= ~WS_EX_APPWINDOW;
						// ex_style |= WS_EX_TOOLWINDOW;
						//::SetWindowLong(hwnd, GWL_EXSTYLE, ex_style);
						//}
						//#endif
					};

					platform_io.Platform_SetWindowPos = [](ImGuiViewport* viewport, ImVec2 pos) -> void
					{
						GLFWwindow* wnd = static_cast<GLFWwindow*>(viewport->PlatformHandle);
						glfwSetWindowPos(wnd, (int)pos.x, (int)pos.y);
					};

					platform_io.Platform_GetWindowPos = [](ImGuiViewport* viewport) -> ImVec2
					{
						int			x = 0, y = 0;
						GLFWwindow* wnd = static_cast<GLFWwindow*>(viewport->PlatformHandle);
						glfwGetWindowPos(wnd, &x, &y);
						return ImVec2((float)x, (float)y);
					};

					platform_io.Platform_SetWindowSize = [](ImGuiViewport* viewport, ImVec2 size) -> void
					{
						GLFWwindow* wnd = static_cast<GLFWwindow*>(viewport->PlatformHandle);
						glfwSetWindowSize(wnd, (int)size.x, (int)size.y);
					};

					platform_io.Platform_GetWindowSize = [](ImGuiViewport* viewport) -> ImVec2
					{
						int			x = 0, y = 0;
						GLFWwindow* wnd = static_cast<GLFWwindow*>(viewport->PlatformHandle);
						glfwGetWindowSize(wnd, &x, &y);
						return ImVec2((float)x, (float)y);
					};

					platform_io.Platform_SetWindowFocus = [](ImGuiViewport* viewport) -> void
					{
						GLFWwindow* wnd = static_cast<GLFWwindow*>(viewport->PlatformHandle);
						glfwFocusWindow(wnd);
					};

					platform_io.Platform_GetWindowFocus = [](ImGuiViewport* viewport) -> bool
					{
						GLFWwindow* wnd = static_cast<GLFWwindow*>(viewport->PlatformHandle);
						return glfwGetWindowAttrib(wnd, GLFW_FOCUSED) != 0;
					};

					platform_io.Platform_GetWindowMinimized = [](ImGuiViewport* viewport) -> bool
					{
						GLFWwindow* wnd = static_cast<GLFWwindow*>(viewport->PlatformHandle);
						return glfwGetWindowAttrib(wnd, GLFW_ICONIFIED) != 0;
					};

					platform_io.Platform_SetWindowTitle = [](ImGuiViewport* viewport, const char* title) -> void
					{
						GLFWwindow* wnd = static_cast<GLFWwindow*>(viewport->PlatformHandle);
						glfwSetWindowTitle(wnd, title);
					};

					platform_io.Platform_SetWindowAlpha = [](ImGuiViewport* viewport, float alpha) -> void
					{
						GLFWwindow* wnd = static_cast<GLFWwindow*>(viewport->PlatformHandle);
						glfwSetWindowOpacity(wnd, alpha);
					};

					//#if HAS_WIN32_IME
					// platform_io.Platform_SetImeInputPos = ImGui_ImplWin32_SetImeInputPos;
					//#endif

					// io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;	// Enable Gamepad Controls
					io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
					io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;	  // Enable Docking
					io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;	  // Enable Multi-Viewport / Platform Windows
					io.ConfigViewportsNoAutoMerge	= false;
					io.ConfigViewportsNoTaskBarIcon = true;

					ImGuiStyle& style				   = ImGui::GetStyle();
					m_application_state->m_timer_ready = false;
				}
				catch (...)
				{
					return MU_LEAF_NEW_ERROR(mu::gfx_error::not_specified{});
				}

				MU_LEAF_CHECK(update_monitors());

				return {};
			}

			[[nodiscard]] auto init_resources() noexcept -> mu::leaf::result<void>
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
						m_diligent_window = std::make_shared<diligent_window>(Diligent::Win32NativeWindow{glfwGetWin32Window(m_window.get())}, m_renderer_globals);

						m_application_state->make_current();

						const auto& swapchain_desc = m_diligent_window->m_swap_chain->GetDesc();
						m_imgui_renderer		   = std::make_shared<Diligent::imgui_renderer>(
							  m_diligent_window->m_globals->m_device,
							  swapchain_desc.ColorBufferFormat,
							  swapchain_desc.DepthBufferFormat,
							  1024 * 1024,
							  1024 * 1024,
							  m_dpi_scale);

						ImGui::GetStyle().ScaleAllSizes(m_dpi_scale);
					}
					catch (...)
					{
						return MU_LEAF_NEW_ERROR(mu::gfx_error::not_specified{});
					}

					MU_LEAF_CHECK(init_window());
				}

				return {};
			}

			virtual auto render(ImDrawData* draw_data) noexcept -> mu::leaf::result<void>
			{
				MU_LEAF_CHECK(m_diligent_window->clear());
				MU_LEAF_CHECK(m_imgui_renderer->render_draw_data(m_diligent_window->m_immediate_context, draw_data));
				return {};
			}

			[[nodiscard]] auto update_mouse() noexcept -> leaf::result<void>
			try
			{
				// Update buttons
				ImGuiIO& io = ImGui::GetIO();
				for (int i = 0; i < IM_ARRAYSIZE(io.MouseDown); i++)
				{
					// If a mouse press event came, always pass it as "mouse held this frame", so we don't miss click-release events that are shorter than 1 frame.
					io.MouseDown[i]								 = m_application_state->m_mouse_just_pressed[i] || glfwGetMouseButton(m_window.get(), i) != 0;
					m_application_state->m_mouse_just_pressed[i] = false;
				}

				// Update mouse position
				const ImVec2 mouse_pos_backup = io.MousePos;
				io.MousePos					  = ImVec2(-FLT_MAX, -FLT_MAX);
				io.MouseHoveredViewport		  = 0;
				ImGuiPlatformIO& platform_io  = ImGui::GetPlatformIO();
				for (int n = 0; n < platform_io.Viewports.Size; n++)
				{
					ImGuiViewport* viewport = platform_io.Viewports[n];
					IM_ASSERT(viewport);

					GLFWwindow* self_window = static_cast<GLFWwindow*>(viewport->PlatformHandle);
					IM_ASSERT(self_window);

					const bool focused = glfwGetWindowAttrib(self_window, GLFW_FOCUSED) != 0;
					if (focused)
					{
						if (io.WantSetMousePos)
						{
							glfwSetCursorPos(self_window, (double)(mouse_pos_backup.x - viewport->Pos.x), (double)(mouse_pos_backup.y - viewport->Pos.y));
						}
						else
						{
							double mouse_x, mouse_y;
							glfwGetCursorPos(self_window, &mouse_x, &mouse_y);
							if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
							{
								// Multi-viewport mode: mouse position in OS absolute coordinates (io.MousePos is (0,0) when the mouse is on the upper-left of the primary
								// monitor)
								int window_x, window_y;
								glfwGetWindowPos(self_window, &window_x, &window_y);
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
						{
							io.MouseDown[i] |= glfwGetMouseButton(self_window, i) != 0;
						}
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
					glfwSetWindowAttrib(self_window, GLFW_MOUSE_PASSTHROUGH, window_no_input);
					if (glfwGetWindowAttrib(self_window, GLFW_HOVERED) && !window_no_input)
					{
						io.MouseHoveredViewport = viewport->ID;
					}
				}

				return {};
			}
			catch (...)
			{
				return MU_LEAF_NEW_ERROR(mu::gfx_error::not_specified{});
			}

			[[nodiscard]] auto update_cursor() noexcept -> leaf::result<void>
			try
			{
				ImGuiIO& io = ImGui::GetIO();
				if ((io.ConfigFlags & ImGuiConfigFlags_NoMouseCursorChange) || glfwGetInputMode(m_window.get(), GLFW_CURSOR) == GLFW_CURSOR_DISABLED)
				{
					return {};
				}

				ImGuiMouseCursor imgui_cursor = ImGui::GetMouseCursor();
				ImGuiPlatformIO& platform_io  = ImGui::GetPlatformIO();
				for (int n = 0; n < platform_io.Viewports.Size; n++)
				{
					ImGuiViewport* viewport = platform_io.Viewports[n];
					IM_ASSERT(viewport);

					GLFWwindow* self_window = static_cast<GLFWwindow*>(viewport->PlatformHandle);
					IM_ASSERT(self_window);

					if (imgui_cursor == ImGuiMouseCursor_None || io.MouseDrawCursor)
					{
						// Hide OS mouse cursor if imgui is drawing it or if it wants no cursor
						glfwSetInputMode(self_window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
					}
					else
					{
						// Show OS mouse cursor
						// FIXME-PLATFORM: Unfocused windows seems to fail changing the mouse cursor with GLFW 3.2, but 3.3 works here.
						glfwSetCursor(
							self_window,
							m_application_state->m_mouse_cursors[imgui_cursor] ? m_application_state->m_mouse_cursors[imgui_cursor]
																			   : m_application_state->m_mouse_cursors[ImGuiMouseCursor_Arrow]);
						glfwSetInputMode(self_window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
					}
				}

				return {};
			}
			catch (...)
			{
				return MU_LEAF_NEW_ERROR(mu::gfx_error::not_specified{});
			}

			[[nodiscard]] auto update_gamepads() noexcept -> leaf::result<void>
			try
			{
				ImGuiIO& io = ImGui::GetIO();
				memset(io.NavInputs, 0, sizeof(io.NavInputs));
				if ((io.ConfigFlags & ImGuiConfigFlags_NavEnableGamepad) == 0)
				{
					return {};
				}

				// Update gamepad inputs
				int					 axes_count = 0, buttons_count = 0;
				const float*		 axes	 = glfwGetJoystickAxes(GLFW_JOYSTICK_1, &axes_count);
				const unsigned char* buttons = glfwGetJoystickButtons(GLFW_JOYSTICK_1, &buttons_count);

				auto MAP_BUTTON = [&](int NAV_NO, int BUTTON_NO)
				{
					if (buttons_count > BUTTON_NO && buttons[BUTTON_NO] == GLFW_PRESS)
					{
						io.NavInputs[NAV_NO] = 1.0f;
					}
				};

				auto MAP_ANALOG = [&](int NAV_NO, int AXIS_NO, float V0, float V1)
				{
					float v = (axes_count > AXIS_NO) ? axes[AXIS_NO] : V0;
					v		= (v - V0) / (V1 - V0);

					if (v > 1.0f)
					{
						v = 1.0f;
					}

					if (io.NavInputs[NAV_NO] < v)
					{
						io.NavInputs[NAV_NO] = v;
					}
				};

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

				if (axes_count > 0 && buttons_count > 0)
				{
					io.BackendFlags |= ImGuiBackendFlags_HasGamepad;
				}
				else
				{
					io.BackendFlags &= ~ImGuiBackendFlags_HasGamepad;
				}

				return {};
			}
			catch (...)
			{
				return MU_LEAF_NEW_ERROR(mu::gfx_error::not_specified{});
			}

			[[nodiscard]] auto update_monitors() noexcept -> leaf::result<void>
			try
			{
				int			  monitors_count = 0;
				GLFWmonitor** glfw_monitors	 = glfwGetMonitors(&monitors_count);

				ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
				platform_io.Monitors.resize(0);
				for (int n = 0; n < monitors_count; n++)
				{
					const GLFWvidmode* vid_mode = glfwGetVideoMode(glfw_monitors[n]);

					glfwSetMonitorUserPointer(glfw_monitors[n], this);

					int x, y;
					glfwGetMonitorPos(glfw_monitors[n], &x, &y);

					int w, h;
					glfwGetMonitorWorkarea(glfw_monitors[n], &x, &y, &w, &h);

					// Warning: the validity of monitor DPI information on Windows depends on the application DPI awareness settings, which generally needs to be set in the
					// manifest or at runtime.
					// float x_scale, y_scale;
					// glfwGetMonitorContentScale(glfw_monitors[n], &x_scale, &y_scale);

					ImGuiPlatformMonitor monitor;
					monitor.MainPos	 = ImVec2((float)x, (float)y);
					monitor.MainSize = ImVec2((float)vid_mode->width, (float)vid_mode->height);
					monitor.WorkPos	 = ImVec2((float)x, (float)y);
					monitor.WorkSize = ImVec2((float)w, (float)h);
					monitor.DpiScale = 1.0f; // std::min(x_scale, y_scale);

					platform_io.Monitors.push_back(monitor);
				}
				m_application_state->m_want_update_monitors = false;

				return {};
			}
			catch (...)
			{
				return MU_LEAF_NEW_ERROR(mu::gfx_error::not_specified{});
			}

			[[nodiscard]] auto new_frame() noexcept -> leaf::result<void>
			{
				time::moment delta_time;
				if (m_application_state->m_timer_ready) [[likely]]
				{
					auto next_time				 = time::now();
					delta_time					 = next_time - m_application_state->m_timer;
					m_application_state->m_timer = next_time;
				}
				else
				{
					m_application_state->m_timer	   = time::now();
					m_application_state->m_timer_ready = true;
				}

				ImGuiIO& io = ImGui::GetIO();
				IM_ASSERT(io.Fonts->IsBuilt() && "Font atlas not built!");

				io.DeltaTime = delta_time.as_seconds<float>();

				// Setup display size (every frame to accommodate for window resizing)
				int w, h;
				glfwGetWindowSize(m_window.get(), &w, &h);
				io.DisplaySize = ImVec2((float)w, (float)h);

				if (m_application_state->m_want_update_monitors)
				{
					MU_LEAF_CHECK(update_monitors());
				}

				MU_LEAF_CHECK(update_mouse());
				MU_LEAF_CHECK(update_cursor());
				MU_LEAF_CHECK(update_gamepads());

				// TODO: Update child windows?

				return {};
			}

			virtual auto begin_frame() noexcept -> mu::leaf::result<void>
			{
				m_application_state->make_current();
				
				MU_LEAF_CHECK(init_resources());

				if (m_diligent_window) [[likely]]
				{
					try
					{
						glfwGetFramebufferSize(m_window.get(), &m_display_size[0], &m_display_size[1]);
					}
					catch (...)
					{
						return MU_LEAF_NEW_ERROR(mu::gfx_error::not_specified{});
					}

					MU_LEAF_CHECK(m_diligent_window->create_resources(m_display_size[0], m_display_size[1]));

					ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
					for (int n = 1; n < platform_io.Viewports.Size; n++)
					{
						ImGuiViewport* viewport = platform_io.Viewports[n];
						IM_ASSERT(viewport);

						if (!(viewport->Flags & ImGuiViewportFlags_Minimized))
						{
							auto wnd = static_cast<gfx_child_window*>(viewport->PlatformUserData);
							wnd->begin_frame(m_diligent_window->m_globals, m_imgui_renderer->m_shared_resources);
						}
					}

					return {};
				}
				else
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

			virtual auto begin_imgui() noexcept -> mu::leaf::result<void>
			{
				try
				{
					m_application_state->make_current();
					
					MU_LEAF_CHECK(new_frame());

					ImGuiViewport* main_viewport = ImGui::GetMainViewport();

					m_dpi_scale = get_dpi_scale_for_hwnd(glfwGetWin32Window(m_window.get()));

					MU_LEAF_CHECK(m_imgui_renderer->new_frame(m_display_size[0], m_display_size[1], Diligent::SURFACE_TRANSFORM::SURFACE_TRANSFORM_OPTIMAL, m_dpi_scale));

					ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
					for (int n = 1; n < platform_io.Viewports.Size; n++)
					{
						ImGuiViewport* viewport = platform_io.Viewports[n];
						IM_ASSERT(viewport);

						if (!(viewport->Flags & ImGuiViewportFlags_Minimized))
						{
							auto wnd = static_cast<gfx_child_window*>(viewport->PlatformUserData);
							wnd->begin_imgui();
						}
					}

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
					m_application_state->make_current();
					
					ImGui::Render();
					ImGui::EndFrame();
					ImGui::UpdatePlatformWindows();
					// ImGuiIO& io = ImGui::GetIO();
					MU_LEAF_CHECK(render(ImGui::GetDrawData()));

					ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
					for (int n = 1; n < platform_io.Viewports.Size; n++)
					{
						ImGuiViewport* viewport = platform_io.Viewports[n];
						IM_ASSERT(viewport);

						if (!(viewport->Flags & ImGuiViewportFlags_Minimized))
						{
							auto wnd = static_cast<gfx_child_window*>(viewport->PlatformUserData);
							wnd->render(m_renderer_globals->m_immediate_context, viewport->DrawData);
						}
					}

					return {};
				}
				catch (...)
				{
					return MU_LEAF_NEW_ERROR(mu::gfx_error::not_specified{});
				}
			}

			auto present() noexcept -> mu::leaf::result<void>
			{
				m_application_state->make_current();
				
				ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();

				if (m_diligent_window && m_diligent_window->m_swap_chain)
				{
					MU_LEAF_CHECK(m_diligent_window->present());
				}

				for (int n = 1; n < platform_io.Viewports.Size; n++)
				{
					ImGuiViewport* viewport = platform_io.Viewports[n];
					IM_ASSERT(viewport);

					if (!(viewport->Flags & ImGuiViewportFlags_Minimized))
					{
						auto wnd = static_cast<gfx_child_window*>(viewport->PlatformUserData);
						wnd->present();
					}
				}
				return {};
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

			[[nodiscard]] auto get_system_ref() noexcept -> mu::leaf::result<std::shared_ptr<glfw_system>>
			try
			{
				if (m_glfw_system.expired()) [[unlikely]]
				{
					auto new_inst = std::make_shared<glfw_system>();
					m_glfw_system = new_inst;
					return new_inst;
				}
				else
				{
					return m_glfw_system.lock();
				}
			}
			catch (...)
			{
				return MU_LEAF_NEW_ERROR(gfx_error::not_specified{});
			}

			gfx_impl() = default;

			virtual ~gfx_impl() = default;

			std::vector<std::weak_ptr<gfx_window_impl>> m_windows;

			virtual auto open_window(int posX, int posY, int sizeX, int sizeY) noexcept -> mu::leaf::result<std::shared_ptr<gfx_window>>
			try
			{
				MU_LEAF_AUTO(system_ref, get_system_ref());
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