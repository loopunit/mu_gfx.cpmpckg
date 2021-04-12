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
				throw MU_LEAF_EXCEPTION(mu::gfx_error::not_specified{});
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

		auto create_resources(int sizeX, int sizeY) noexcept -> mu::leaf::result<void>
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
			return MU_LEAF_NEW_ERROR(mu::gfx_error::not_specified{});
		}

		auto clear() noexcept -> mu::leaf::result<void>
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
			return MU_LEAF_NEW_ERROR(mu::gfx_error::not_specified{});
		}

		auto render() noexcept -> mu::leaf::result<void>
		{
			MU_LEAF_CHECK(clear());
			return {};
		}

		auto present() noexcept -> mu::leaf::result<void>
		try
		{
			m_pSwapChain->Present();
			return {};
		}
		catch (...)
		{
			return MU_LEAF_NEW_ERROR(mu::gfx_error::not_specified{});
		}

		diligent_window(Diligent::Win32NativeWindow native_wnd, std::shared_ptr<diligent_globals> globals)
		{
			m_globals = globals;

			Diligent::SwapChainDesc SCDesc;
			m_globals->m_pFactory->CreateSwapChainD3D12(
				m_globals->m_pDevice, m_globals->m_pImmediateContext, SCDesc, Diligent::FullScreenModeDesc{}, native_wnd, &m_pSwapChain);
		}

		~diligent_window()
		{
			m_pSwapChain.Release();
			m_globals.reset();
		}
	};
}