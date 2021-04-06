#include "mu_gfx_impl.h"

#include <optional>

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

#include "imgui_renderer.h"

////

namespace mu
{
	// TODO: glfw error type using glfwGetError(const char** description);

	struct diligent_globals
	{
		Diligent::RefCntAutoPtr<Diligent::IRenderDevice>	   m_pDevice;
		Diligent::RefCntAutoPtr<Diligent::IDeviceContext>	   m_pImmediateContext;
		Diligent::RefCntAutoPtr<Diligent::IEngineFactoryD3D12> m_pFactory;

		diligent_globals()
		{
			Diligent::EngineD3D12CreateInfo EngineCI;
			m_pFactory = Diligent::GetEngineFactoryD3D12();
			try
			{
				m_pFactory->CreateDeviceAndContextsD3D12(EngineCI, &m_pDevice, &m_pImmediateContext);
			}
			catch (...)
			{
				m_pFactory.Release();
				throw mu::leaf::exception(mu::gfx_error::not_specified{});
			}
		}

		~diligent_globals()
		{
			try
			{
				m_pImmediateContext.Release();
			}
			catch (...)
			{
				// TODO: report
			}
			// TODO: exception will be re-thrown

			try
			{
				m_pDevice.Release();
			}
			catch (...)
			{
				// TODO: report
			}
			// TODO: exception will be re-thrown

			try
			{
				m_pFactory.Release();
			}
			catch (...)
			{
				// TODO: report
			}
			// TODO: exception will be re-thrown
		}
	};

	struct diligent_window
	{
		std::shared_ptr<diligent_globals>			  m_globals;
		Diligent::RefCntAutoPtr<Diligent::ISwapChain> m_pSwapChain;

		mu::leaf::result<void> create_resources(int sizeX, int sizeY) noexcept
		try
		{
			const auto& swapchain_desc = m_pSwapChain->GetDesc();
			if (swapchain_desc.Width != sizeX || swapchain_desc.Height != sizeY)
			{
				m_pSwapChain->Resize(sizeX, sizeY);
			}

			return {};
		}
		catch (...)
		{
			return mu::leaf::new_error(mu::gfx_error::not_specified{});
		}

		mu::leaf::result<void> clear() noexcept
		try
		{
			// Set render targets before issuing any draw command.
			// Note that Present() unbinds the back buffer if it is set as render target.
			Diligent::ITextureView* last_backbuffer_rtv	 = m_pSwapChain->GetCurrentBackBufferRTV();
			Diligent::ITextureView* last_depthbuffer_rtv = m_pSwapChain->GetDepthBufferDSV();
			m_globals->m_pImmediateContext->SetRenderTargets(1, &last_backbuffer_rtv, last_depthbuffer_rtv, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

			// Clear the back buffer
			const float ClearColor[] = {0.350f, 0.350f, 0.350f, 1.000f};

			// Let the engine perform required state transitions
			m_globals->m_pImmediateContext->ClearRenderTarget(last_backbuffer_rtv, ClearColor, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
			m_globals->m_pImmediateContext->ClearDepthStencil(last_depthbuffer_rtv, Diligent::CLEAR_DEPTH_FLAG, 1.f, 0, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

			return {};
		}
		catch (...)
		{
			return mu::leaf::new_error(mu::gfx_error::not_specified{});
		}

		mu::leaf::result<void> render() noexcept
		{
			MU_LEAF_CHECK(clear());
			return {};
		}

		mu::leaf::result<void> present() noexcept
		try
		{
			m_pSwapChain->Present();
			return {};
		}
		catch (...)
		{
			return mu::leaf::new_error(mu::gfx_error::not_specified{});
		}

		mu::leaf::result<void> init(Diligent::Win32NativeWindow native_wnd, std::shared_ptr<diligent_globals> globals) noexcept
		{
			try
			{
				m_globals = globals;

				Diligent::SwapChainDesc SCDesc;
				m_globals->m_pFactory->CreateSwapChainD3D12(
					m_globals->m_pDevice, m_globals->m_pImmediateContext, SCDesc, Diligent::FullScreenModeDesc{}, native_wnd, &m_pSwapChain);
			}
			catch (...)
			{
				return mu::leaf::new_error(mu::gfx_error::not_specified{});
			}

			return {};
		}

		mu::leaf::result<void> destroy() noexcept
		try
		{
			m_pSwapChain.Release();
			m_globals.reset();
			return {};
		}
		catch (...)
		{
			return mu::leaf::new_error(mu::gfx_error::not_specified{});
		}
	};

	struct glfw_system
	{
	private:
		int m_glfw_status = GLFW_FALSE;

	public:
		glfw_system() = default;

		~glfw_system()
		{
			if (m_glfw_status == GLFW_TRUE) [[unlikely]]
			{
				shutdown();
			}
		}

		leaf::result<void> init() noexcept
		try
		{
			if (m_glfw_status != GLFW_TRUE) [[likely]]
			{
				m_glfw_status = glfwInit();

				if (m_glfw_status != GLFW_TRUE) [[unlikely]]
				{
					return leaf::new_error(gfx_error::not_specified{});
				}
				return {};
			}
			else
			{
				return leaf::new_error(gfx_error::not_specified{});
			}
		}
		catch (...)
		{
			return leaf::new_error(gfx_error::not_specified{});
		}

		leaf::result<void> shutdown() noexcept
		try
		{
			if (m_glfw_status == GLFW_TRUE) [[likely]]
			{
				m_glfw_status = GLFW_FALSE;

				glfwTerminate();
				return {};
			}
			else
			{
				return leaf::new_error(gfx_error::not_specified{});
			}
		}
		catch (...)
		{
			return leaf::new_error(gfx_error::not_specified{});
		}

		leaf::result<void> poll() noexcept
		try
		{
			if (m_glfw_status == GLFW_TRUE) [[likely]]
			{
				glfwPollEvents();
				return {};
			}
			else
			{
				return leaf::new_error(gfx_error::not_specified{});
			}
		}
		catch (...)
		{
			return leaf::new_error(gfx_error::not_specified{});
		}

		leaf::result<GLFWwindow*> create_window(int posX, int posY, int sizeX, int sizeY) noexcept
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
					return leaf::new_error(gfx_error::not_specified{});
				}
			}

			return leaf::new_error(gfx_error::not_specified{});
		}
		catch (...)
		{
		}

		leaf::result<void> destroy_window(GLFWwindow* wnd) noexcept
		try
		{
			if (wnd != nullptr) [[likely]]
			{
				glfwDestroyWindow(wnd);
				return {};
			}

			return leaf::new_error(gfx_error::not_specified{});
		}
		catch (...)
		{
			return leaf::new_error(gfx_error::not_specified{});
		}
	};

	struct glfw_window : public mu_gfx_window
	{
		std::shared_ptr<glfw_system>	 m_glfw_system;
		GLFWwindow*						 m_window = nullptr;
		std::shared_ptr<diligent_window> m_diligent_window;

		glfw_window(std::shared_ptr<glfw_system> glfw_sys) : m_glfw_system(glfw_sys) { }

		virtual ~glfw_window()
		{
			destroy();
		}

		leaf::result<void> init(int posX, int posY, int sizeX, int sizeY) noexcept
		{
			if (m_window) [[unlikely]]
			{
				return leaf::new_error(gfx_error::not_specified{});
			}

			MU_LEAF_AUTO(new_window, m_glfw_system->create_window(posX, posY, sizeX, sizeY));
			m_window = new_window;

			try
			{
				glfwSetWindowUserPointer(m_window, this);
			}
			catch (...)
			{
				return leaf::new_error(gfx_error::not_specified{}, destroy());
			}

			return {};
		}

		leaf::result<void> destroy() noexcept
		{
			if (m_renderer_globals) [[likely]]
			{
				m_renderer_globals.reset();
			}

			if (m_window) [[likely]]
			{
				MU_LEAF_CHECK(m_glfw_system->destroy_window(m_window));
				m_window = nullptr;
			}

			return {};
		}

		virtual mu::leaf::result<bool> wants_to_close() noexcept
		try
		{
			return glfwWindowShouldClose(m_window) != 0;
		}
		catch (...)
		{
			return leaf::new_error(gfx_error::not_specified{});
		}

		virtual mu::leaf::result<void> show() noexcept
		try
		{
			glfwShowWindow(m_window);
			return {};
		}
		catch (...)
		{
			return leaf::new_error(gfx_error::not_specified{});
		}

		std::shared_ptr<diligent_globals> m_renderer_globals;
		mu::leaf::result<void>			  begin_frame(std::shared_ptr<diligent_globals> renderer_globals)
		{
			if (!m_renderer_globals) [[unlikely]]
			{
				m_renderer_globals = renderer_globals;
			}

			if (m_renderer_globals) [[likely]]
			{
				if (!m_diligent_window) [[unlikely]]
				{
					auto dw = std::make_shared<diligent_window>();
					MU_LEAF_CHECK(dw->init(Diligent::Win32NativeWindow{glfwGetWin32Window(m_window)}, m_renderer_globals));
					m_diligent_window = dw;
				}

				if (m_diligent_window) [[likely]]
				{
					int display_w, display_h;
					glfwGetFramebufferSize(m_window, &display_w, &display_h);
					m_diligent_window->create_resources(display_w, display_h);
					m_diligent_window->clear();
					return {};
				}
			}

			return leaf::new_error(gfx_error::not_specified{});
		}

		mu::leaf::result<void> end_frame()
		{
			return {};
		}

		mu::leaf::result<void> present()
		{
			m_diligent_window->present();
			return {};
		}
	};
} // namespace mu

struct mu_gfx_impl : public mu_gfx_interface
{
	std::shared_ptr<mu::glfw_system>	  m_glfw_system;
	std::shared_ptr<mu::diligent_globals> m_diligent_globals;
	using window_list = std::vector<std::shared_ptr<mu_gfx_window>>;
	window_list		m_window_list;
	mu_gfx_renderer m_gfx_renderer;

	static inline mu_gfx_impl* singleton() noexcept
	{
		return reinterpret_cast<mu_gfx_impl*>(mu_gfx().get());
	}

	virtual mu::leaf::result<void> select_platform() noexcept
	{
		return {};
	}

	virtual mu::leaf::result<void> init() noexcept
	try
	{
		auto glfw_system = std::make_shared<mu::glfw_system>();
		MU_LEAF_CHECK(glfw_system->init());
		m_glfw_system = std::move(glfw_system);

		return {};
	}
	catch (...)
	{
		return mu::leaf::new_error(mu::gfx_error::not_specified{});
	}

	virtual mu::leaf::result<void> pump() noexcept
	{
		if (m_glfw_system)
		{
			MU_LEAF_CHECK(m_glfw_system->poll());

			for (auto itor = m_window_list.begin(); itor != m_window_list.end();)
			{
				MU_LEAF_AUTO(wants_close, (*itor)->wants_to_close());
				if (wants_close)
				{
					itor = m_window_list.erase(itor);
				}
				else
				{
					++itor;
				}
			}

			return {};
		}

		return mu::leaf::new_error(mu::gfx_error::not_specified{});
	}

	virtual mu::leaf::result<void> destroy() noexcept
	try
	{
		m_glfw_system.reset();
		return {};
	}
	catch (...)
	{
		return mu::leaf::new_error(mu::gfx_error::not_specified{});
	}

	virtual mu::leaf::result<std::shared_ptr<mu_gfx_window>> open_window(int posX, int posY, int sizeX, int sizeY) noexcept
	try
	{
		if (m_glfw_system)
		{
			auto new_window = std::make_shared<mu::glfw_window>(m_glfw_system);
			if (auto res = new_window->init(posX, posY, sizeX, sizeY))
			{
				m_window_list.push_back(new_window);
				return new_window;
			}
			else
			{
				return res.error();
			}
		}

		return mu::leaf::new_error(mu::gfx_error::not_specified{});
	}
	catch (...)
	{
		return mu::leaf::new_error(mu::gfx_error::not_specified{});
	}

	virtual mu::leaf::result<mu_gfx_renderer_ref> begin_window(std::shared_ptr<mu_gfx_window> h) noexcept
	{
		if (!m_diligent_globals)
		{
			try
			{
				m_diligent_globals = std::make_shared<mu::diligent_globals>();
			}
			catch (...)
			{
				return mu::leaf::new_error(mu::gfx_error::not_specified{});
			}
		}

		auto active_window = std::static_pointer_cast<mu::glfw_window>(h);

		MU_LEAF_CHECK(active_window->begin_frame(m_diligent_globals));

		m_gfx_renderer.m_window = h;
		return mu_gfx_renderer_ref(&m_gfx_renderer, &mu_gfx_renderer::release);
	}

	virtual mu::leaf::result<void> present() noexcept
	{
		for (auto itor = m_window_list.begin(); itor != m_window_list.end(); ++itor)
		{
			auto wnd = std::static_pointer_cast<mu::glfw_window>(*itor);
			wnd->present();
		}

		return {};
	}

	void release_renderer(mu_gfx_renderer* r) noexcept
	{
		if (r->m_window) [[likely]]
		{
			auto active_window = std::static_pointer_cast<mu::glfw_window>(r->m_window);
			active_window->end_frame();
			active_window.reset();
			r->m_window.reset();
		}
	}
};

void mu_gfx_renderer::release(mu_gfx_renderer* r) noexcept
{
	mu_gfx_impl::singleton()->release_renderer(r);
}

mu::leaf::result<void> mu_gfx_renderer::draw_test() noexcept
{
	return {};
}

///

MU_DEFINE_VIRTUAL_SINGLETON(mu_gfx_interface, mu_gfx_impl);
MU_EXPORT_SINGLETON(mu_gfx);
