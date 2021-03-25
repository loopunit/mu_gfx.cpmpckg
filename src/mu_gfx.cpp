#include "mu_gfx_impl.h"

#define NOMINMAX

#include <GLFW/glfw3.h>
#ifdef _WIN32
#undef APIENTRY
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h> // for glfwGetWin32Window
#endif

#include <Graphics/GraphicsEngineD3D12/interface/EngineFactoryD3D12.h>
#include <Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <Graphics/GraphicsEngine/interface/SwapChain.h>
#include <Common/interface/RefCntAutoPtr.hpp>

#if USING_NUKLEAR
#include "nuklear_renderer.h"
#endif

#include "imgui.h"

////

namespace mu
{
	struct imgui_globals
	{
		ImGuiContext* m_imgui = nullptr;

		leaf::result<void> init() noexcept
		{
			if (m_imgui = ImGui::CreateContext(); m_imgui == nullptr)
			{
				return leaf::new_error(gfx_error::not_specified{});
			}
			return {};
		}

		virtual leaf::result<void> destroy() noexcept
		{
			if (m_imgui)
			{
				ImGui::DestroyContext(m_imgui);
				m_imgui = nullptr;
			}
			return {};
		}

		////

		virtual leaf::result<void> make_current() noexcept
		{
			if (m_imgui)
			{
				ImGui::SetCurrentContext(m_imgui);
				return {};
			}
			return leaf::new_error(gfx_error::not_specified{});
		}
	};
} // namespace mu

struct mu_gfx_impl : public mu_gfx_interface
{
	struct globals
	{
		Diligent::RefCntAutoPtr<Diligent::IRenderDevice>	   m_pDevice;
		Diligent::RefCntAutoPtr<Diligent::IDeviceContext>	   m_pImmediateContext;
		Diligent::RefCntAutoPtr<Diligent::IEngineFactoryD3D12> m_pFactory;
#if USING_NUKLEAR
		nk_diligent_globals* m_nk_globals;
#endif // #if USING_NUKLEAR

		mu::leaf::result<void> init() noexcept
		{
			try
			{
				Diligent::EngineD3D12CreateInfo EngineCI;
				m_pFactory = Diligent::GetEngineFactoryD3D12();
				m_pFactory->CreateDeviceAndContextsD3D12(EngineCI, &m_pDevice, &m_pImmediateContext);
			}
			catch (...)
			{
				return mu::leaf::new_error(mu::gfx_error::not_specified{});
			}

			return {};
		}

		mu::leaf::result<void> destroy() noexcept
		{
			try
			{
				m_pImmediateContext.Release();
				m_pDevice.Release();
			}
			catch (...)
			{
				return mu::leaf::new_error(mu::gfx_error::not_specified{});
			}

			return {};
		}
	};

	struct window
	{
		GLFWwindow*									  m_window = nullptr;
		Diligent::RefCntAutoPtr<Diligent::ISwapChain> m_pSwapChain;
		bool										  m_is_primary = false;
#if USING_NUKLEAR
		nk_diligent_context* m_nk_context = nullptr;
#endif // #if USING_NUKLEAR

		mu::leaf::result<void> create_resources(globals* glob) noexcept
		{
			try
			{
#if USING_NUKLEAR
				if (!m_nk_context)
				{
					int display_w, display_h;
					glfwGetFramebufferSize(m_window, &display_w, &display_h);

					if (m_is_primary)
					{
						glob->m_nk_globals = nk_diligent_init_globals(glob->m_pDevice);
					}

					m_nk_context = nk_diligent_init(
						glob->m_nk_globals,
						display_w,
						display_h,
						m_pSwapChain->GetDesc().ColorBufferFormat,
						m_pSwapChain->GetDesc().DepthBufferFormat,
						1024 * 32,
						1024 * 32 * 3);

					nk_diligent_init_resources(m_nk_context, glob->m_nk_globals, glob->m_pImmediateContext);
				}
#endif // #if USING_NUKLEAR
			}
			catch (...)
			{
				return mu::leaf::new_error(mu::gfx_error::not_specified{});
			}
			return {};
		}

		mu::leaf::result<void> clear(globals* glob) noexcept
		{
			try
			{
				// Set render targets before issuing any draw command.
				// Note that Present() unbinds the back buffer if it is set as render target.
				Diligent::ITextureView* last_backbuffer_rtv	 = m_pSwapChain->GetCurrentBackBufferRTV();
				Diligent::ITextureView* last_depthbuffer_rtv = m_pSwapChain->GetDepthBufferDSV();
				glob->m_pImmediateContext->SetRenderTargets(1, &last_backbuffer_rtv, last_depthbuffer_rtv, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

				// Clear the back buffer
				const float ClearColor[] = {0.350f, 0.350f, 0.350f, 1.0f};
				// Let the engine perform required state transitions
				glob->m_pImmediateContext->ClearRenderTarget(last_backbuffer_rtv, ClearColor, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
				glob->m_pImmediateContext->ClearDepthStencil(last_depthbuffer_rtv, Diligent::CLEAR_DEPTH_FLAG, 1.f, 0, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
			}
			catch (...)
			{
				return mu::leaf::new_error(mu::gfx_error::not_specified{});
			}

			return {};
		}

		mu::leaf::result<void> render(globals* glob) noexcept
		{
			BOOST_LEAF_CHECK(clear(glob));

			try
			{
#if USING_NUKLEAR
				nk_diligent_render(m_nk_context, glob->m_pImmediateContext, false);
#endif // #if USING_NUKLEAR
			}
			catch (...)
			{
				return mu::leaf::new_error(mu::gfx_error::not_specified{});
			}

			return {};
		}

		mu::leaf::result<void> present(globals* glob) noexcept
		{
			try
			{
				m_pSwapChain->Present();
			}
			catch (...)
			{
				return mu::leaf::new_error(mu::gfx_error::not_specified{});
			}

			return {};
		}

		mu::leaf::result<void> on_resize(globals* glob, int sizeX, int sizeY) noexcept
		{
			try
			{
				int display_w, display_h;
				glfwGetFramebufferSize(m_window, &display_w, &display_h);
				m_pSwapChain->Resize(display_w, display_h);
#if USING_NUKLEAR
				nk_diligent_resize(m_nk_context, glob->m_nk_globals, glob->m_pImmediateContext, display_w, display_h);
#endif // #if USING_NUKLEAR
			}
			catch (...)
			{
				return mu::leaf::new_error(mu::gfx_error::not_specified{});
			}

			BOOST_LEAF_CHECK(clear(glob));

			try
			{
#if USING_NUKLEAR
				nk_diligent_render(m_nk_context, glob->m_pImmediateContext, true);
#endif // #if USING_NUKLEAR
			}
			catch (...)
			{
				return mu::leaf::new_error(mu::gfx_error::not_specified{});
			}

			BOOST_LEAF_CHECK(present(glob));

			return {};
		}

		mu::leaf::result<void> create_window(int posX, int posY, int sizeX, int sizeY) noexcept
		{
			try
			{
				glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
				m_window = glfwCreateWindow(sizeX, sizeY, "", NULL, NULL);

				glfwSetWindowUserPointer(m_window, this);

				glfwShowWindow(m_window);
			}
			catch (...)
			{
				return mu::leaf::new_error(mu::gfx_error::not_specified{});
			}

			return {};
		}

		mu::leaf::result<void> init(globals* glob) noexcept
		{
			try
			{
				Diligent::SwapChainDesc		SCDesc;
				Diligent::Win32NativeWindow Window{glfwGetWin32Window(m_window)};
				glob->m_pFactory->CreateSwapChainD3D12(glob->m_pDevice, glob->m_pImmediateContext, SCDesc, Diligent::FullScreenModeDesc{}, Window, &m_pSwapChain);
			}
			catch (...)
			{
				return mu::leaf::new_error(mu::gfx_error::not_specified{});
			}

			return {};
		}

		mu::leaf::result<void> destroy(globals* glob) noexcept
		{
			try
			{
#if USING_NUKLEAR
				nk_diligent_shutdown(m_nk_context);
				m_nk_context = nullptr;
#endif // #if USING_NUKLEAR

				if (m_is_primary)
				{
#if USING_NUKLEAR
					nk_diligent_shutdown_globals(glob->m_nk_globals);
					glob->m_nk_globals = nullptr;
#endif // #if USING_NUKLEAR
				}

				m_pSwapChain.Release();
			}
			catch (...)
			{
				return mu::leaf::new_error(mu::gfx_error::not_specified{});
			}

			return {};
		}

		mu::leaf::result<void> destroy_window() noexcept
		{
			try
			{
				glfwDestroyWindow(m_window);
				m_window = nullptr;
			}
			catch (...)
			{
				return mu::leaf::new_error(mu::gfx_error::not_specified{});
			}

			return {};
		}
	};

	std::unique_ptr<globals>		   m_globals;
	std::list<std::unique_ptr<window>> m_windows;

	mu::leaf::result<void> init_primary_window(int posX, int posY, int sizeX, int sizeY) noexcept
	{
		try
		{
			m_globals		= std::make_unique<globals>();
			auto new_window = std::make_unique<window>();

			new_window->m_is_primary = true;
			BOOST_LEAF_CHECK(new_window->create_window(posX, posY, sizeX, sizeY));

			m_globals->init();
			BOOST_LEAF_CHECK(new_window->init(m_globals.get()));

			glfwSetWindowSizeCallback(
				new_window->m_window,
				[](GLFWwindow* src_window, int a, int b) -> void
				{
					auto wnd = reinterpret_cast<window*>(glfwGetWindowUserPointer(src_window));
					wnd->on_resize(singleton()->m_globals.get(), a, b);
				});

			// glfwSetWindowCloseCallback(
			//	new_window->m_window,
			//	[](GLFWwindow* window) -> void
			//	{
			//		// viewport->PlatformRequestClose = true;
			//	});

			m_windows.push_back(std::move(new_window));
		}
		catch (...)
		{
			return mu::leaf::new_error(mu::gfx_error::not_specified{});
		}

		return {};
	}

	mu::leaf::result<void> init_secondary_window(int posX, int posY, int sizeX, int sizeY) noexcept
	{
		try
		{
			auto new_window			 = std::make_unique<window>();
			new_window->m_is_primary = true;
			BOOST_LEAF_CHECK(new_window->create_window(posX, posY, sizeX, sizeY));

			BOOST_LEAF_CHECK(new_window->init(m_globals.get()));

			glfwSetWindowSizeCallback(
				new_window->m_window,
				[](GLFWwindow* src_window, int a, int b) -> void
				{
					auto wnd = reinterpret_cast<window*>(glfwGetWindowUserPointer(src_window));
					wnd->on_resize(singleton()->m_globals.get(), a, b);
				});

			glfwSetWindowCloseCallback(
				new_window->m_window,
				[](GLFWwindow* src_window) -> void
				{
					auto wnd = reinterpret_cast<window*>(glfwGetWindowUserPointer(src_window));
					singleton()->close_secondary_window(wnd);
				});

			m_windows.push_back(std::move(new_window));
		}
		catch (...)
		{
			return mu::leaf::new_error(mu::gfx_error::not_specified{});
		}

		return {};
	}

	mu::leaf::result<void> close_secondary_window(window* wnd) noexcept
	{
		try
		{
			for (auto itor = m_windows.begin(); itor != m_windows.end(); ++itor)
			{
				if ((*itor).get() == wnd)
				{
					BOOST_LEAF_CHECK(wnd->destroy(m_globals.get()));
					BOOST_LEAF_CHECK(wnd->destroy_window());
					m_windows.erase(itor);
					break;
				}
			}
		}
		catch (...)
		{
			return mu::leaf::new_error(mu::gfx_error::not_specified{});
		}

		return {};
	}

	////

	static inline mu_gfx_impl* singleton()
	{
		return reinterpret_cast<mu_gfx_impl*>(mu_gfx().get());
	}

	virtual mu::leaf::result<void> select_platform() noexcept
	{
		return {};
	}

	virtual mu::leaf::result<void> init() noexcept
	{
		try
		{
			if (!glfwInit())
			{
				return mu::leaf::new_error(mu::gfx_error::not_specified{});
			}
		}
		catch (...)
		{
			return mu::leaf::new_error(mu::gfx_error::not_specified{});
		}

		BOOST_LEAF_CHECK(init_primary_window(100, 100, 1280, 800));
		BOOST_LEAF_CHECK(init_secondary_window(200, 200, 640, 480));

		return {};
	}

	virtual mu::leaf::result<bool> pump() noexcept
	{
		glfwPollEvents();

		for (auto& wnd : m_windows)
		{
			if (wnd->m_is_primary)
			{
				return !glfwWindowShouldClose(wnd->m_window);
			}
		}
		return true;
	}

	virtual mu::leaf::result<void> begin_frame() noexcept
	{
		for (auto& wnd : m_windows)
		{
			BOOST_LEAF_CHECK(wnd->create_resources(m_globals.get()));
		}

		return {};
	}

	virtual mu::leaf::result<void> end_frame() noexcept
	{
		try
		{
			for (auto& wnd : m_windows)
			{
				BOOST_LEAF_CHECK(wnd->render(m_globals.get()));
			}

			for (auto& wnd : m_windows)
			{
				BOOST_LEAF_CHECK(wnd->present(m_globals.get()));
			}
		}
		catch (...)
		{
			return mu::leaf::new_error(mu::gfx_error::not_specified{});
		}

		return {};
	}

	virtual mu::leaf::result<void> destroy() noexcept
	{
		try
		{
			for (auto& wnd : m_windows)
			{
				BOOST_LEAF_CHECK(wnd->destroy(m_globals.get()));
			}

			BOOST_LEAF_CHECK(m_globals->destroy());

			for (auto& wnd : m_windows)
			{
				BOOST_LEAF_CHECK(wnd->destroy_window());
			}

			glfwTerminate();
		}
		catch (...)
		{
			return mu::leaf::new_error(mu::gfx_error::not_specified{});
		}

		return {};
	}
};

///

MU_DEFINE_VIRTUAL_SINGLETON(mu_gfx_interface, mu_gfx_impl);
MU_EXPORT_SINGLETON(mu_gfx);
