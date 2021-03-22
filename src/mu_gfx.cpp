#include "mu_gfx_impl.h"

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

struct mu_gfx_impl : public mu_gfx_interface
{
	static inline const char* VSSource = R"(
    struct PSInput 
    { 
        float4 Pos   : SV_POSITION; 
        float3 Color : COLOR; 
    };
    void main(in  uint    VertId : SV_VertexID,
            out PSInput PSIn) 
    {
        float4 Pos[3];
        Pos[0] = float4(-0.5, -0.5, 0.0, 1.0);
        Pos[1] = float4( 0.0, +0.5, 0.0, 1.0);
        Pos[2] = float4(+0.5, -0.5, 0.0, 1.0);
        float3 Col[3];
        Col[0] = float3(1.0, 0.0, 0.0); // red
        Col[1] = float3(0.0, 1.0, 0.0); // green
        Col[2] = float3(0.0, 0.0, 1.0); // blue
        PSIn.Pos   = Pos[VertId];
        PSIn.Color = Col[VertId];
    }
    )";

	// Pixel shader simply outputs interpolated vertex color
	static inline const char* PSSource = R"(
    struct PSInput 
    { 
        float4 Pos   : SV_POSITION; 
        float3 Color : COLOR; 
    };
    struct PSOutput
    { 
        float4 Color : SV_TARGET; 
    };
    void main(in  PSInput  PSIn,
            out PSOutput PSOut)
    {
        PSOut.Color = float4(PSIn.Color.rgb, 1.0);
    }
    )";

	struct globals
	{
		Diligent::RefCntAutoPtr<Diligent::IRenderDevice>	   m_pDevice;
		Diligent::RefCntAutoPtr<Diligent::IDeviceContext>	   m_pImmediateContext;
		Diligent::RefCntAutoPtr<Diligent::IEngineFactoryD3D12> m_pFactory;

		void init()
		{
			Diligent::EngineD3D12CreateInfo EngineCI;
			m_pFactory = Diligent::GetEngineFactoryD3D12();
			m_pFactory->CreateDeviceAndContextsD3D12(EngineCI, &m_pDevice, &m_pImmediateContext);
		}

		void destroy()
		{
			m_pImmediateContext.Release();
			m_pDevice.Release();
		}
	};

	struct window
	{
		GLFWwindow*										  m_window = nullptr;
		Diligent::RefCntAutoPtr<Diligent::ISwapChain>	  m_pSwapChain;
		Diligent::RefCntAutoPtr<Diligent::IPipelineState> m_pPSO;
		bool											  m_is_primary = false;

		void create_resources(globals* glob)
		{
			if (!m_pPSO)
			{
				// Pipeline state object encompasses configuration of all GPU stages

				Diligent::GraphicsPipelineStateCreateInfo PSOCreateInfo;

				// Pipeline state name is used by the engine to report issues.
				// It is always a good idea to give objects descriptive names.
				PSOCreateInfo.PSODesc.Name = "Simple triangle PSO";

				// This is a graphics pipeline
				PSOCreateInfo.PSODesc.PipelineType = Diligent::PIPELINE_TYPE_GRAPHICS;

				// clang-format off
				// This tutorial will render to a single render target
				PSOCreateInfo.GraphicsPipeline.NumRenderTargets             = 1;
				// Set render target format which is the format of the swap chain's color buffer
				PSOCreateInfo.GraphicsPipeline.RTVFormats[0]                = m_pSwapChain->GetDesc().ColorBufferFormat;
				// Use the depth buffer format from the swap chain
				PSOCreateInfo.GraphicsPipeline.DSVFormat                    = m_pSwapChain->GetDesc().DepthBufferFormat;
				// Primitive topology defines what kind of primitives will be rendered by this pipeline state
				PSOCreateInfo.GraphicsPipeline.PrimitiveTopology            = Diligent::PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
				// No back face culling for this tutorial
				PSOCreateInfo.GraphicsPipeline.RasterizerDesc.CullMode      = Diligent::CULL_MODE_NONE;
				// Disable depth testing
				PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthEnable = Diligent::False;
				// clang-format on

				Diligent::ShaderCreateInfo ShaderCI;
				// Tell the system that the shader source code is in HLSL.
				// For OpenGL, the engine will convert this into GLSL under the hood
				ShaderCI.SourceLanguage = Diligent::SHADER_SOURCE_LANGUAGE_HLSL;
				// OpenGL backend requires emulated combined HLSL texture samplers (g_Texture + g_Texture_sampler combination)
				ShaderCI.UseCombinedTextureSamplers = true;
				// Create a vertex shader
				Diligent::RefCntAutoPtr<Diligent::IShader> pVS;
				{
					ShaderCI.Desc.ShaderType = Diligent::SHADER_TYPE_VERTEX;
					ShaderCI.EntryPoint		 = "main";
					ShaderCI.Desc.Name		 = "Triangle vertex shader";
					ShaderCI.Source			 = VSSource;
					glob->m_pDevice->CreateShader(ShaderCI, &pVS);
				}

				// Create a pixel shader
				Diligent::RefCntAutoPtr<Diligent::IShader> pPS;
				{
					ShaderCI.Desc.ShaderType = Diligent::SHADER_TYPE_PIXEL;
					ShaderCI.EntryPoint		 = "main";
					ShaderCI.Desc.Name		 = "Triangle pixel shader";
					ShaderCI.Source			 = PSSource;
					glob->m_pDevice->CreateShader(ShaderCI, &pPS);
				}

				// Finally, create the pipeline state
				PSOCreateInfo.pVS = pVS;
				PSOCreateInfo.pPS = pPS;
				glob->m_pDevice->CreateGraphicsPipelineState(PSOCreateInfo, &m_pPSO);
			}
		}

		void clear(globals* glob)
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

		void render(globals* glob)
		{
			clear(glob);

			// Set the pipeline state in the immediate context
			glob->m_pImmediateContext->SetPipelineState(m_pPSO);

			// Typically we should now call CommitShaderResources(), however shaders in this example don't
			// use any resources.

			Diligent::DrawAttribs drawAttrs;
			drawAttrs.NumVertices = 3; // Render 3 vertices
			glob->m_pImmediateContext->Draw(drawAttrs);
		}

		void present(globals* glob)
		{
			m_pSwapChain->Present();
		}

		void on_resize(globals* glob, int sizeX, int sizeY)
		{
			int display_w, display_h;
			glfwGetFramebufferSize(m_window, &display_w, &display_h);
			m_pSwapChain->Resize(display_w, display_h);
			clear(glob);
			present(glob);
		}

		void create_window(int posX, int posY, int sizeX, int sizeY)
		{
			glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
			m_window = glfwCreateWindow(sizeX, sizeY, "", NULL, NULL);

			glfwSetWindowUserPointer(m_window, this);

			glfwShowWindow(m_window);
		}

		void init(globals* glob)
		{
			Diligent::SwapChainDesc		SCDesc;
			Diligent::Win32NativeWindow Window{glfwGetWin32Window(m_window)};
			glob->m_pFactory->CreateSwapChainD3D12(glob->m_pDevice, glob->m_pImmediateContext, SCDesc, Diligent::FullScreenModeDesc{}, Window, &m_pSwapChain);
		}

		void destroy(globals* glob)
		{
			m_pSwapChain.Release();
			m_pPSO.Release();
		}

		void destroy_window()
		{
			glfwDestroyWindow(m_window);
			m_window = nullptr;
		}
	};

	std::unique_ptr<globals>		   m_globals;
	std::list<std::unique_ptr<window>> m_windows;

	mu::leaf::result<void> init_primary_window(int posX, int posY, int sizeX, int sizeY) noexcept
	{
		if (!glfwInit())
		{
			return mu::leaf::new_error();
			//("glfw creation failed");
		}

		m_globals		= std::make_unique<globals>();
		auto new_window = std::make_unique<window>();

		new_window->m_is_primary = true;
		new_window->create_window(posX, posY, sizeX, sizeY);

		m_globals->init();
		new_window->init(m_globals.get());

		glfwSetWindowSizeCallback(
			new_window->m_window,
			[](GLFWwindow* src_window, int a, int b) -> void
			{
				auto wnd = reinterpret_cast<window*>(glfwGetWindowUserPointer(src_window));
				wnd->on_resize(singleton()->m_globals.get(), a, b);
			});

		glfwSetWindowCloseCallback(
			new_window->m_window,
			[](GLFWwindow* window) -> void
			{
				// viewport->PlatformRequestClose = true;
			});

		m_windows.push_back(std::move(new_window));

		return {};
	}

	mu::leaf::result<void> init_secondary_window(int posX, int posY, int sizeX, int sizeY) noexcept
	{
		auto new_window = std::make_unique<window>();

		new_window->m_is_primary = true;
		new_window->create_window(posX, posY, sizeX, sizeY);

		new_window->init(m_globals.get());

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

		return {};
	}

	void close_secondary_window(window* wnd)
	{
		for (auto itor = m_windows.begin(); itor != m_windows.end(); ++itor)
		{
			if ((*itor).get() == wnd)
			{
				wnd->destroy(m_globals.get());
				wnd->destroy_window();
				m_windows.erase(itor);
				break;
			}
		}
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
			wnd->create_resources(m_globals.get());
		}

		return {};
	}

	virtual mu::leaf::result<void> end_frame() noexcept
	{
		for (auto& wnd : m_windows)
		{
			wnd->render(m_globals.get());
		}

		for (auto& wnd : m_windows)
		{
			wnd->present(m_globals.get());
		}
		return {};
	}

	virtual mu::leaf::result<void> destroy() noexcept
	{
		for (auto& wnd : m_windows)
		{
			wnd->destroy(m_globals.get());
		}

		m_globals->destroy();

		for (auto& wnd : m_windows)
		{
			wnd->destroy_window();
		}

		glfwTerminate();

		return {};
	}
};

///

MU_DEFINE_VIRTUAL_SINGLETON(mu_gfx_interface, mu_gfx_impl);
MU_EXPORT_SINGLETON(mu_gfx);
