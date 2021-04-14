#include "mu_gfx_impl.h"

#define NOMINMAX

#include <GLFW/glfw3.h>
#ifdef _WIN32
#undef APIENTRY
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h> // for glfwGetWin32Window
#endif

#include "mu_diligent.h"

#include <foonathan/memory/container.hpp>
#include <foonathan/memory/memory_pool.hpp>
#include <foonathan/memory/memory_stack.hpp>
#include <foonathan/memory/smart_ptr.hpp>
#include <foonathan/memory/static_allocator.hpp>
#include <foonathan/memory/temporary_allocator.hpp>
#include <foonathan/memory/namespace_alias.hpp>

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

			auto get_renderer_globals() noexcept -> std::shared_ptr<diligent_globals>
			{
				auto system_ref = (m_renderer_globals.expired()) ? std::make_shared<diligent_globals>() : m_renderer_globals.lock();
				if (m_renderer_globals.expired()) [[unlikely]]
				{
					m_renderer_globals = system_ref;
				}
				return system_ref;
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
			std::shared_ptr<Diligent::ImGuiDiligentRenderer> m_imgui_renderer;

			imgui_context(std::shared_ptr<diligent_window> diligent_window) : m_diligent_window(diligent_window), m_imgui_context(ImGui::CreateContext(), ImGui::DestroyContext)
			{
				const auto& swapchain_desc = m_diligent_window->m_pSwapChain->GetDesc();

				ImGui::SetCurrentContext(m_imgui_context.get());

				m_imgui_renderer = std::make_shared<Diligent::ImGuiDiligentRenderer>(
					m_diligent_window->m_globals->m_pDevice,
					swapchain_desc.ColorBufferFormat,
					swapchain_desc.DepthBufferFormat,
					1024 * 1024,
					1024 * 1024);
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
		};

		struct gfx_window_impl : public gfx_window
		{
			using frame_stack = memory::memory_stack<>;

			struct gfx_renderer_impl : public gfx_renderer
			{
				std::shared_ptr<imgui_context> m_imgui_context;
				std::shared_ptr<frame_stack>   m_frame_stack;
				frame_stack::marker			   m_frame_stack_top;

				std::shared_ptr<gfx_window_impl>  m_self_window;
				std::shared_ptr<diligent_globals> m_renderer_globals;
				std::shared_ptr<diligent_window>  m_diligent_window;

				gfx_renderer_impl(std::shared_ptr<imgui_context> ic, std::shared_ptr<frame_stack> fs, frame_stack::marker fs_top)
					: m_imgui_context(ic)
					, m_frame_stack(fs)
					, m_frame_stack_top(fs_top)
				{ }

				static inline void release(gfx_renderer* r) noexcept
				try
				{
					if (auto res = r->end_frame(); !res) [[unlikely]]
					{
						MU_LEAF_LOG_ERROR(gfx_error::not_specified{});
					}

					auto stack_ref = ((gfx_renderer_impl*)r)->m_frame_stack;
					auto stack_top = ((gfx_renderer_impl*)r)->m_frame_stack_top;
					r->~gfx_renderer();
					stack_ref->unwind(stack_top);
				}
				catch (...)
				{
					MU_LEAF_LOG_ERROR(gfx_error::not_specified{});
					return;
				}

				virtual ~gfx_renderer_impl() { }

				virtual auto test() noexcept -> mu::leaf::result<void>
				{
					try
					{
						ImGui::SetCurrentContext(m_imgui_context->m_imgui_context.get());
						ImGui::ShowDemoWindow();
						return {};
					}
					catch (...)
					{
						return MU_LEAF_NEW_ERROR(mu::gfx_error::not_specified{});
					}
				}

				auto begin() noexcept -> mu::leaf::result<void>
				{
					int display_w, display_h;
					try
					{
						glfwGetFramebufferSize(m_self_window->m_window, &display_w, &display_h);
					}
					catch (...)
					{
						return MU_LEAF_NEW_ERROR(mu::gfx_error::not_specified{});
					}

					m_diligent_window->create_resources(display_w, display_h);
					m_diligent_window->clear();

					ImGui::SetCurrentContext(m_imgui_context->m_imgui_context.get());

					ImGuiIO& io	   = ImGui::GetIO();
					io.DisplaySize = ImVec2(float(display_w), float(display_h));

					m_imgui_context->m_imgui_renderer->NewFrame(display_w, display_h, Diligent::SURFACE_TRANSFORM::SURFACE_TRANSFORM_OPTIMAL);
					ImGui::NewFrame();

					return {};
				}

				virtual auto end_frame() noexcept -> mu::leaf::result<void>
				try
				{
					ImGui::SetCurrentContext(m_imgui_context->m_imgui_context.get());
					ImGui::Render();
					m_imgui_context->m_imgui_renderer->RenderDrawData(m_renderer_globals->m_pImmediateContext, ImGui::GetDrawData());
					ImGui::EndFrame();
					return {};
				}
				catch (...)
				{
					return MU_LEAF_NEW_ERROR(gfx_error::not_specified{});
				}
			};

			std::shared_ptr<glfw_system> m_glfw_system;
			GLFWwindow*					 m_window = nullptr;

			std::shared_ptr<frame_stack>	  m_frame_stack;
			frame_stack::marker				  m_frame_stack_top;
			std::shared_ptr<diligent_window>  m_diligent_window;
			std::shared_ptr<diligent_globals> m_renderer_globals;
			std::shared_ptr<imgui_context>	  m_imgui_context;

			auto create_window(int posX, int posY, int sizeX, int sizeY) noexcept -> leaf::result<GLFWwindow*>
			try
			{
				glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
				if (auto wnd = glfwCreateWindow(sizeX, sizeY, "", NULL, NULL); wnd != nullptr) [[likely]]
				{
					try
					{
						glfwSetWindowPos(wnd, posX, posY);
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

			gfx_window_impl(std::shared_ptr<glfw_system> sys, int posX, int posY, int sizeX, int sizeY)
				: m_glfw_system(sys)
				, m_frame_stack(std::make_unique<frame_stack>(4096))
				, m_frame_stack_top(m_frame_stack->top())
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

			virtual auto wants_to_close() noexcept -> mu::leaf::result<bool>
			try
			{
				return glfwWindowShouldClose(m_window) != 0;
			}
			catch (...)
			{
				return MU_LEAF_NEW_ERROR(gfx_error::not_specified{});
			}

			virtual auto show() noexcept -> mu::leaf::result<void>
			try
			{
				glfwShowWindow(m_window);
				return {};
			}
			catch (...)
			{
				return MU_LEAF_NEW_ERROR(gfx_error::not_specified{});
			}

			virtual auto begin_window() noexcept -> mu::leaf::result<renderer_ref>
			{
				if (m_frame_stack->top() == m_frame_stack_top) [[likely]]
				{
					if (!m_renderer_globals) [[unlikely]]
					{
						m_renderer_globals = m_glfw_system->get_renderer_globals();
					}

					if (m_renderer_globals) [[likely]]
					{
						if (!m_diligent_window) [[unlikely]]
						{
							try
							{
								m_diligent_window = std::make_shared<diligent_window>(Diligent::Win32NativeWindow{glfwGetWin32Window(m_window)}, m_renderer_globals);
								m_imgui_context	  = std::make_shared<imgui_context>(m_diligent_window);
							}
							catch (...)
							{
								return MU_LEAF_NEW_ERROR(mu::gfx_error::not_specified{});
							}
						}

						if (m_diligent_window) [[likely]]
						{
							auto stack_root = m_frame_stack->top();
							auto impl_mem	= m_frame_stack->allocate(sizeof(gfx_renderer_impl), 64);
							auto impl_ptr	= new (impl_mem) gfx_renderer_impl(m_imgui_context, m_frame_stack, m_frame_stack_top);

							impl_ptr->m_self_window		 = static_pointer_cast<gfx_window_impl>(shared_self());
							impl_ptr->m_renderer_globals = m_renderer_globals;
							impl_ptr->m_diligent_window	 = m_diligent_window;

							MU_LEAF_CHECK(impl_ptr->begin());

							return renderer_ref(impl_ptr, gfx_renderer_impl::release);
						}
						else
						{
							return MU_LEAF_NEW_ERROR(mu::gfx_error::not_specified{});
						}
					}
					else
					{
						return MU_LEAF_NEW_ERROR(mu::gfx_error::not_specified{});
					}
				}
				else
				{
					// There was a renderer already in flight
					return MU_LEAF_NEW_ERROR(mu::gfx_error::not_specified{});
				}
			}

			auto present() noexcept -> mu::leaf::result<void>
			{
				MU_LEAF_CHECK(m_diligent_window->present());
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
			using frame_stack = memory::memory_stack<>;

			struct pumper_impl : public gfx_pumper
			{
				std::shared_ptr<frame_stack> m_frame_stack;
				frame_stack::marker			 m_frame_stack_top;

				pumper_impl(std::shared_ptr<frame_stack> fs, frame_stack::marker fs_top) : m_frame_stack(fs), m_frame_stack_top(fs_top) { }

				static inline void release(gfx_pumper* r) noexcept
				try
				{
					if (auto res = r->present(); !res) [[unlikely]]
					{
						MU_LEAF_LOG_ERROR(gfx_error::not_specified{});
					}

					auto stack_ref = ((pumper_impl*)r)->m_frame_stack;
					auto stack_top = ((pumper_impl*)r)->m_frame_stack_top;
					r->~gfx_pumper();
					stack_ref->unwind(stack_top);
				}
				catch (...)
				{
					MU_LEAF_LOG_ERROR(gfx_error::not_specified{});
					return;
				}

				virtual ~pumper_impl() { }

				virtual auto present() noexcept -> mu::leaf::result<void>
				try
				{
					return gfx()->present();
				}
				catch (...)
				{
					return MU_LEAF_NEW_ERROR(gfx_error::not_specified{});
				}
			};

			std::shared_ptr<frame_stack>				m_frame_stack;
			frame_stack::marker							m_frame_stack_top;
			std::weak_ptr<glfw_system>					m_glfw_system;
			std::vector<std::weak_ptr<gfx_window_impl>> m_windows;

			auto get_system_ref() noexcept -> std::shared_ptr<glfw_system>
			{
				auto system_ref = (m_glfw_system.expired()) ? std::make_shared<glfw_system>() : m_glfw_system.lock();
				if (m_glfw_system.expired()) [[unlikely]]
				{
					m_glfw_system = system_ref;
				}
				return system_ref;
			}

			gfx_impl() : m_frame_stack(std::make_unique<frame_stack>(4096)), m_frame_stack_top(m_frame_stack->top()) { }

			virtual ~gfx_impl() { }

			virtual auto select_platform() noexcept -> leaf::result<bool>
			try
			{
				return true;
			}
			catch (...)
			{
				return MU_LEAF_NEW_ERROR(mu::gfx_error::not_specified{});
			}

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

			virtual auto pump() noexcept -> mu::leaf::result<pumper_ref>
			try
			{
				if (!m_glfw_system.expired()) [[likely]]
				{
					if (m_frame_stack->top() == m_frame_stack_top) [[likely]]
					{
						auto stack_root = m_frame_stack->top();
						auto impl_mem	= m_frame_stack->allocate(sizeof(pumper_impl), 64);
						auto impl_ptr	= new (impl_mem) pumper_impl(m_frame_stack, m_frame_stack_top);
						glfwPollEvents();
						return pumper_ref(impl_ptr, pumper_impl::release);
					}
					else
					{
						return MU_LEAF_NEW_ERROR(gfx_error::not_specified{});
					}
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
