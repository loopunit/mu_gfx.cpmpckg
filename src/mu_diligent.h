#pragma once

#include <Graphics/GraphicsEngineD3D12/interface/EngineFactoryD3D12.h>
#include <Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <Graphics/GraphicsEngine/interface/DeviceContext.h>
#include <Graphics/GraphicsEngine/interface/SwapChain.h>
#include <Common/interface/RefCntAutoPtr.hpp>

namespace mu
{
	// TODO: glfw error type using glfwGetError(const char** description);

	struct diligent_globals
	{
		Diligent::RefCntAutoPtr<Diligent::IRenderDevice>	   m_device;
		Diligent::RefCntAutoPtr<Diligent::IDeviceContext>	   m_immediate_context;
		Diligent::RefCntAutoPtr<Diligent::IEngineFactoryD3D12> m_engine_factory;

		diligent_globals()
		{
			m_engine_factory = Diligent::GetEngineFactoryD3D12();

			Diligent::EngineD3D12CreateInfo EngineCI;
			m_engine_factory->CreateDeviceAndContextsD3D12(EngineCI, &m_device, &m_immediate_context);
		}

		~diligent_globals()
		{
			try
			{
				// m_immediate_context.Release();
			}
			catch (...)
			{
				MU_LEAF_LOG_ERROR(mu::gfx_error::not_specified{});
			}

			try
			{
				m_device.Release();
			}
			catch (...)
			{
				MU_LEAF_LOG_ERROR(mu::gfx_error::not_specified{});
			}

			try
			{
				m_engine_factory.Release();
			}
			catch (...)
			{
				MU_LEAF_LOG_ERROR(mu::gfx_error::not_specified{});
			}
		}
	};

	struct diligent_window
	{
		std::shared_ptr<diligent_globals>				  m_globals;
		Diligent::RefCntAutoPtr<Diligent::IDeviceContext> m_immediate_context;
		Diligent::RefCntAutoPtr<Diligent::ISwapChain>	  m_swap_chain;

		auto create_resources(int sizeX, int sizeY) noexcept -> mu::leaf::result<void>
		try
		{
			const auto& swapchain_desc = m_swap_chain->GetDesc();
			if (swapchain_desc.Width != sizeX || swapchain_desc.Height != sizeY)
			{
				m_swap_chain->Resize(sizeX, sizeY);
			}

			return {};
		}
		catch (...)
		{
			return MU_LEAF_NEW_ERROR(mu::gfx_error::not_specified{});
		}

		auto clear() noexcept -> mu::leaf::result<void>
		try
		{
			// Set render targets before issuing any draw command.
			// Note that Present() unbinds the back buffer if it is set as render target.
			Diligent::ITextureView* last_backbuffer_rtv	 = m_swap_chain->GetCurrentBackBufferRTV();
			Diligent::ITextureView* last_depthbuffer_rtv = m_swap_chain->GetDepthBufferDSV();
			m_immediate_context->SetRenderTargets(1, &last_backbuffer_rtv, last_depthbuffer_rtv, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

			// Clear the back buffer
			const float clear_color[] = {0.350f, 0.350f, 0.350f, 1.000f};

			// Let the engine perform required state transitions
			m_immediate_context->ClearRenderTarget(last_backbuffer_rtv, clear_color, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
			m_immediate_context->ClearDepthStencil(last_depthbuffer_rtv, Diligent::CLEAR_DEPTH_FLAG, 1.f, 0, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

			return {};
		}
		catch (...)
		{
			return MU_LEAF_NEW_ERROR(mu::gfx_error::not_specified{});
		}

		auto present() noexcept -> mu::leaf::result<void>
		try
		{
			m_swap_chain->Present();
			return {};
		}
		catch (...)
		{
			return MU_LEAF_NEW_ERROR(mu::gfx_error::not_specified{});
		}

		diligent_window(Diligent::Win32NativeWindow native_wnd, std::shared_ptr<diligent_globals> globals)
		{
			m_globals			= globals;
			m_immediate_context = m_globals->m_immediate_context;

			Diligent::SwapChainDesc swapchain_desc;
			m_globals->m_engine_factory->CreateSwapChainD3D12(m_globals->m_device, m_immediate_context, swapchain_desc, Diligent::FullScreenModeDesc{}, native_wnd, &m_swap_chain);
		}

		~diligent_window()
		{
			m_swap_chain.Release();
			m_globals.reset();
		}
	};
} // namespace mu