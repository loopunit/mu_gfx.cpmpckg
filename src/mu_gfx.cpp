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
		BOOST_LEAF_CHECK(init(100.0f, 100.0f, 1280.0f, 800.0f));
		return {};
	}

	virtual mu::leaf::result<bool> pump() noexcept
	{
		glfwPollEvents();
		return !glfwWindowShouldClose(m_window);
	}

	virtual mu::leaf::result<void> begin_frame() noexcept
	{
        create_resources();
        return {};
	}

	virtual mu::leaf::result<void> end_frame() noexcept
	{
        render();
        present();
		return {};
	}

	virtual mu::leaf::result<void> destroy() noexcept
	{
		destroy_window();

		return {};
	}

	////

	GLFWwindow* m_window = nullptr;

	Diligent::RefCntAutoPtr<Diligent::IRenderDevice>  m_pDevice;
	Diligent::RefCntAutoPtr<Diligent::IDeviceContext> m_pImmediateContext;
	Diligent::RefCntAutoPtr<Diligent::ISwapChain>	  m_pSwapChain;
	Diligent::RefCntAutoPtr<Diligent::IPipelineState> m_pPSO;

	void show_window()
	{
		glfwShowWindow(m_window);
	}

	void create_resources()
	{
		if (m_pPSO)
        {
            return;
        }

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
			m_pDevice->CreateShader(ShaderCI, &pVS);
		}

		// Create a pixel shader
		Diligent::RefCntAutoPtr<Diligent::IShader> pPS;
		{
			ShaderCI.Desc.ShaderType = Diligent::SHADER_TYPE_PIXEL;
			ShaderCI.EntryPoint		 = "main";
			ShaderCI.Desc.Name		 = "Triangle pixel shader";
			ShaderCI.Source			 = PSSource;
			m_pDevice->CreateShader(ShaderCI, &pPS);
		}

		// Finally, create the pipeline state
		PSOCreateInfo.pVS = pVS;
		PSOCreateInfo.pPS = pPS;
		m_pDevice->CreateGraphicsPipelineState(PSOCreateInfo, &m_pPSO);
	}

	void render()
	{
		// Set render targets before issuing any draw command.
		// Note that Present() unbinds the back buffer if it is set as render target.
		auto* pRTV = m_pSwapChain->GetCurrentBackBufferRTV();
		auto* pDSV = m_pSwapChain->GetDepthBufferDSV();
		m_pImmediateContext->SetRenderTargets(1, &pRTV, pDSV, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

		// Clear the back buffer
		const float ClearColor[] = {0.350f, 0.350f, 0.350f, 1.0f};
		// Let the engine perform required state transitions
		m_pImmediateContext->ClearRenderTarget(pRTV, ClearColor, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
		m_pImmediateContext->ClearDepthStencil(pDSV, Diligent::CLEAR_DEPTH_FLAG, 1.f, 0, Diligent::RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

		// Set the pipeline state in the immediate context
		m_pImmediateContext->SetPipelineState(m_pPSO);

		// Typically we should now call CommitShaderResources(), however shaders in this example don't
		// use any resources.

		Diligent::DrawAttribs drawAttrs;
		drawAttrs.NumVertices = 3; // Render 3 vertices
		m_pImmediateContext->Draw(drawAttrs);
	}

    void present()
    {
        m_pSwapChain->Present();
    }

    void window_resize(uint32_t Width, uint32_t Height)
    {
        if (m_pSwapChain)
            m_pSwapChain->Resize(Width, Height);
    }

	mu::leaf::result<void> init(int posX, int posY, int sizeX, int sizeY) noexcept
	{
		if (!glfwInit())
		{
			return mu::leaf::new_error();
			//("glfw creation failed");
		}

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		m_window = glfwCreateWindow(sizeX, sizeY, "", NULL, NULL);

		show_window();

		Diligent::SwapChainDesc			SCDesc;
		Diligent::EngineD3D12CreateInfo EngineCI;
		auto*							pFactoryD3D12 = Diligent::GetEngineFactoryD3D12();

		pFactoryD3D12->CreateDeviceAndContextsD3D12(EngineCI, &m_pDevice, &m_pImmediateContext);
		Diligent::Win32NativeWindow Window{glfwGetWin32Window(m_window)};
		pFactoryD3D12->CreateSwapChainD3D12(m_pDevice, m_pImmediateContext, SCDesc, Diligent::FullScreenModeDesc{}, Window, &m_pSwapChain);

		return {};
	}

	void destroy_window()
	{
		glfwDestroyWindow(m_window);
		m_window = nullptr;
	}
};

///

MU_DEFINE_VIRTUAL_SINGLETON(mu_gfx_interface, mu_gfx_impl);
MU_EXPORT_SINGLETON(mu_gfx);

#if 0
			
			

#endif