/*
 *  Copyright 2019-2021 Diligent Graphics LLC
 *  Copyright 2015-2019 Egor Yusov
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 *  In no event and under no legal theory, whether in tort (including negligence),
 *  contract, or otherwise, unless required by applicable law (such as deliberate
 *  and grossly negligent acts) or agreed to in writing, shall any Contributor be
 *  liable for any damages, including any direct, indirect, special, incidental,
 *  or consequential damages of any character arising as a result of this License or
 *  out of the use or inability to use the software (including but not limited to damages
 *  for loss of goodwill, work stoppage, computer failure or malfunction, or any and
 *  all other commercial damages or losses), even if such Contributor has been advised
 *  of the possibility of such damages.
 */

#include <cstddef>
#include "imgui_renderer.h"

#include <Graphics/GraphicsTools/interface/MapHelper.hpp>
#include <Graphics/GraphicsEngine/interface/RenderDevice.h>
#include <Graphics/GraphicsEngine/interface/DeviceContext.h>

namespace Diligent
{
	static const char* g_vertex_shader_hlsl = R"(
cbuffer Constants
{
    float4x4 ProjectionMatrix;
}

struct VSInput
{
    float2 pos : ATTRIB0;
    float2 uv  : ATTRIB1;
    float4 col : ATTRIB2;
};

struct PSInput
{
    float4 pos : SV_POSITION;
    float4 col : COLOR;
    float2 uv  : TEXCOORD;
};

void main(in VSInput VSIn, out PSInput PSIn)
{
    PSIn.pos = mul(ProjectionMatrix, float4(VSIn.pos.xy, 0.0, 1.0));
    PSIn.col = VSIn.col;
    PSIn.uv  = VSIn.uv;
}
)";

	static const char* g_pixel_shader_hlsl = R"(
struct PSInput
{
    float4 pos : SV_POSITION;
    float4 col : COLOR;
    float2 uv  : TEXCOORD;
};

Texture2D    Texture;
SamplerState Texture_sampler;

float4 main(in PSInput PSIn) : SV_Target
{
    return PSIn.col * Texture.Sample(Texture_sampler, PSIn.uv);
}
)";

	static const char* g_vertex_shader_glsl = R"(
#ifdef VULKAN
#   define BINDING(X) layout(binding=X)
#   define OUT_LOCATION(X) layout(location=X) // Requires separable programs
#else
#   define BINDING(X)
#   define OUT_LOCATION(X)
#endif
BINDING(0) uniform Constants
{
    mat4 ProjectionMatrix;
};

layout(location = 0) in vec2 in_pos;
layout(location = 1) in vec2 in_uv;
layout(location = 2) in vec4 in_col;

OUT_LOCATION(0) out vec4 vsout_col;
OUT_LOCATION(1) out vec2 vsout_uv;

#ifndef GL_ES
out gl_PerVertex
{
    vec4 gl_Position;
};
#endif

void main()
{
    gl_Position = ProjectionMatrix * vec4(in_pos.xy, 0.0, 1.0);
    vsout_col = in_col;
    vsout_uv  = in_uv;
}
)";

	static const char* g_pixel_shader_glsl = R"(
#ifdef VULKAN
#   define BINDING(X) layout(binding=X)
#   define IN_LOCATION(X) layout(location=X) // Requires separable programs
#else
#   define BINDING(X)
#   define IN_LOCATION(X)
#endif
BINDING(0) uniform sampler2D Texture;

IN_LOCATION(0) in vec4 vsout_col;
IN_LOCATION(1) in vec2 vsout_uv;

layout(location = 0) out vec4 psout_col;

void main()
{
    psout_col = vsout_col * texture(Texture, vsout_uv);
}
)";

	// glslangValidator.exe -V -e main --vn VertexShader_SPIRV ImGUI.vert

	static constexpr uint32_t g_vertex_shader_spirv[] = {
		0x07230203, 0x00010000, 0x0008000a, 0x00000028, 0x00000000, 0x00020011, 0x00000001, 0x0006000b, 0x00000001, 0x4c534c47, 0x6474732e, 0x3035342e, 0x00000000, 0x0003000e,
		0x00000000, 0x00000001, 0x000b000f, 0x00000000, 0x00000004, 0x6e69616d, 0x00000000, 0x0000000a, 0x00000016, 0x00000020, 0x00000022, 0x00000025, 0x00000026, 0x00030003,
		0x00000002, 0x000001a4, 0x00040005, 0x00000004, 0x6e69616d, 0x00000000, 0x00060005, 0x00000008, 0x505f6c67, 0x65567265, 0x78657472, 0x00000000, 0x00060006, 0x00000008,
		0x00000000, 0x505f6c67, 0x7469736f, 0x006e6f69, 0x00030005, 0x0000000a, 0x00000000, 0x00050005, 0x0000000e, 0x736e6f43, 0x746e6174, 0x00000073, 0x00080006, 0x0000000e,
		0x00000000, 0x6a6f7250, 0x69746365, 0x614d6e6f, 0x78697274, 0x00000000, 0x00030005, 0x00000010, 0x00000000, 0x00040005, 0x00000016, 0x705f6e69, 0x0000736f, 0x00050005,
		0x00000020, 0x756f7376, 0x6f635f74, 0x0000006c, 0x00040005, 0x00000022, 0x635f6e69, 0x00006c6f, 0x00050005, 0x00000025, 0x756f7376, 0x76755f74, 0x00000000, 0x00040005,
		0x00000026, 0x755f6e69, 0x00000076, 0x00050048, 0x00000008, 0x00000000, 0x0000000b, 0x00000000, 0x00030047, 0x00000008, 0x00000002, 0x00040048, 0x0000000e, 0x00000000,
		0x00000005, 0x00050048, 0x0000000e, 0x00000000, 0x00000023, 0x00000000, 0x00050048, 0x0000000e, 0x00000000, 0x00000007, 0x00000010, 0x00030047, 0x0000000e, 0x00000002,
		0x00040047, 0x00000010, 0x00000022, 0x00000000, 0x00040047, 0x00000010, 0x00000021, 0x00000000, 0x00040047, 0x00000016, 0x0000001e, 0x00000000, 0x00040047, 0x00000020,
		0x0000001e, 0x00000000, 0x00040047, 0x00000022, 0x0000001e, 0x00000002, 0x00040047, 0x00000025, 0x0000001e, 0x00000001, 0x00040047, 0x00000026, 0x0000001e, 0x00000001,
		0x00020013, 0x00000002, 0x00030021, 0x00000003, 0x00000002, 0x00030016, 0x00000006, 0x00000020, 0x00040017, 0x00000007, 0x00000006, 0x00000004, 0x0003001e, 0x00000008,
		0x00000007, 0x00040020, 0x00000009, 0x00000003, 0x00000008, 0x0004003b, 0x00000009, 0x0000000a, 0x00000003, 0x00040015, 0x0000000b, 0x00000020, 0x00000001, 0x0004002b,
		0x0000000b, 0x0000000c, 0x00000000, 0x00040018, 0x0000000d, 0x00000007, 0x00000004, 0x0003001e, 0x0000000e, 0x0000000d, 0x00040020, 0x0000000f, 0x00000002, 0x0000000e,
		0x0004003b, 0x0000000f, 0x00000010, 0x00000002, 0x00040020, 0x00000011, 0x00000002, 0x0000000d, 0x00040017, 0x00000014, 0x00000006, 0x00000002, 0x00040020, 0x00000015,
		0x00000001, 0x00000014, 0x0004003b, 0x00000015, 0x00000016, 0x00000001, 0x0004002b, 0x00000006, 0x00000018, 0x00000000, 0x0004002b, 0x00000006, 0x00000019, 0x3f800000,
		0x00040020, 0x0000001e, 0x00000003, 0x00000007, 0x0004003b, 0x0000001e, 0x00000020, 0x00000003, 0x00040020, 0x00000021, 0x00000001, 0x00000007, 0x0004003b, 0x00000021,
		0x00000022, 0x00000001, 0x00040020, 0x00000024, 0x00000003, 0x00000014, 0x0004003b, 0x00000024, 0x00000025, 0x00000003, 0x0004003b, 0x00000015, 0x00000026, 0x00000001,
		0x00050036, 0x00000002, 0x00000004, 0x00000000, 0x00000003, 0x000200f8, 0x00000005, 0x00050041, 0x00000011, 0x00000012, 0x00000010, 0x0000000c, 0x0004003d, 0x0000000d,
		0x00000013, 0x00000012, 0x0004003d, 0x00000014, 0x00000017, 0x00000016, 0x00050051, 0x00000006, 0x0000001a, 0x00000017, 0x00000000, 0x00050051, 0x00000006, 0x0000001b,
		0x00000017, 0x00000001, 0x00070050, 0x00000007, 0x0000001c, 0x0000001a, 0x0000001b, 0x00000018, 0x00000019, 0x00050091, 0x00000007, 0x0000001d, 0x00000013, 0x0000001c,
		0x00050041, 0x0000001e, 0x0000001f, 0x0000000a, 0x0000000c, 0x0003003e, 0x0000001f, 0x0000001d, 0x0004003d, 0x00000007, 0x00000023, 0x00000022, 0x0003003e, 0x00000020,
		0x00000023, 0x0004003d, 0x00000014, 0x00000027, 0x00000026, 0x0003003e, 0x00000025, 0x00000027, 0x000100fd, 0x00010038};

	static constexpr uint32_t g_fragment_shader_spirv[] = {
		0x07230203, 0x00010000, 0x0008000a, 0x00000018, 0x00000000, 0x00020011, 0x00000001, 0x0006000b, 0x00000001, 0x4c534c47, 0x6474732e, 0x3035342e, 0x00000000, 0x0003000e,
		0x00000000, 0x00000001, 0x0008000f, 0x00000004, 0x00000004, 0x6e69616d, 0x00000000, 0x00000009, 0x0000000b, 0x00000014, 0x00030010, 0x00000004, 0x00000007, 0x00030003,
		0x00000002, 0x000001a4, 0x00040005, 0x00000004, 0x6e69616d, 0x00000000, 0x00050005, 0x00000009, 0x756f7370, 0x6f635f74, 0x0000006c, 0x00050005, 0x0000000b, 0x756f7376,
		0x6f635f74, 0x0000006c, 0x00040005, 0x00000010, 0x74786554, 0x00657275, 0x00050005, 0x00000014, 0x756f7376, 0x76755f74, 0x00000000, 0x00040047, 0x00000009, 0x0000001e,
		0x00000000, 0x00040047, 0x0000000b, 0x0000001e, 0x00000000, 0x00040047, 0x00000010, 0x00000022, 0x00000000, 0x00040047, 0x00000010, 0x00000021, 0x00000000, 0x00040047,
		0x00000014, 0x0000001e, 0x00000001, 0x00020013, 0x00000002, 0x00030021, 0x00000003, 0x00000002, 0x00030016, 0x00000006, 0x00000020, 0x00040017, 0x00000007, 0x00000006,
		0x00000004, 0x00040020, 0x00000008, 0x00000003, 0x00000007, 0x0004003b, 0x00000008, 0x00000009, 0x00000003, 0x00040020, 0x0000000a, 0x00000001, 0x00000007, 0x0004003b,
		0x0000000a, 0x0000000b, 0x00000001, 0x00090019, 0x0000000d, 0x00000006, 0x00000001, 0x00000000, 0x00000000, 0x00000000, 0x00000001, 0x00000000, 0x0003001b, 0x0000000e,
		0x0000000d, 0x00040020, 0x0000000f, 0x00000000, 0x0000000e, 0x0004003b, 0x0000000f, 0x00000010, 0x00000000, 0x00040017, 0x00000012, 0x00000006, 0x00000002, 0x00040020,
		0x00000013, 0x00000001, 0x00000012, 0x0004003b, 0x00000013, 0x00000014, 0x00000001, 0x00050036, 0x00000002, 0x00000004, 0x00000000, 0x00000003, 0x000200f8, 0x00000005,
		0x0004003d, 0x00000007, 0x0000000c, 0x0000000b, 0x0004003d, 0x0000000e, 0x00000011, 0x00000010, 0x0004003d, 0x00000012, 0x00000015, 0x00000014, 0x00050057, 0x00000007,
		0x00000016, 0x00000011, 0x00000015, 0x00050085, 0x00000007, 0x00000017, 0x0000000c, 0x00000016, 0x0003003e, 0x00000009, 0x00000017, 0x000100fd, 0x00010038};

	static const char* g_shaders_msl = R"(
#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

struct VSConstants
{
    float4x4 ProjectionMatrix;
};

struct VSIn
{
    float2 pos [[attribute(0)]];
    float2 uv  [[attribute(1)]];
    float4 col [[attribute(2)]];
};

struct VSOut
{
    float4 col [[user(locn0)]];
    float2 uv  [[user(locn1)]];
    float4 pos [[position]];
};

vertex VSOut vs_main(VSIn in [[stage_in]], constant VSConstants& Constants [[buffer(0)]])
{
    VSOut out = {};
    out.pos = Constants.ProjectionMatrix * float4(in.pos, 0.0, 1.0);
    out.col = in.col;
    out.uv  = in.uv;
    return out;
}

struct PSOut
{
    float4 col [[color(0)]];
};

fragment PSOut ps_main(VSOut in [[stage_in]],
                       texture2d<float> Texture [[texture(0)]],
                       sampler Texture_sampler  [[sampler(0)]])
{
    PSOut out = {};
    out.col = in.col * Texture.sample(Texture_sampler, in.uv);
    return out;
}
)";

	imgui_renderer::imgui_renderer(
		IRenderDevice* device,
		TEXTURE_FORMAT back_buffer_fmt,
		TEXTURE_FORMAT depth_buffer_fmt,
		Uint32		   initial_vertex_buffer_size,
		Uint32		   initial_index_buffer_size,
		float		   scale)
		: m_device{device}
		, m_back_buffer_fmt{back_buffer_fmt}
		, m_depth_buffer_fmt{depth_buffer_fmt}
		, m_vertex_buffer_size{initial_vertex_buffer_size}
		, m_index_buffer_size{initial_index_buffer_size}
		, m_scale{scale}
	{
		IMGUI_CHECKVERSION();
		ImGuiIO& io			   = ImGui::GetIO();
		io.BackendRendererName = "imgui_renderer";
		io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset; // We can honor the ImDrawCmd::VtxOffset field, allowing for large meshes.

		create_device_objects(scale, true);
	}

	imgui_renderer::~imgui_renderer() { }

	void imgui_renderer::new_frame(Uint32 render_surface_width, Uint32 render_surface_height, SURFACE_TRANSFORM surface_pre_transform, float scale)
	{
		create_device_objects(scale, false);

		m_render_surface_width	= render_surface_width;
		m_render_surface_height = render_surface_height;
		m_surface_pre_transform = surface_pre_transform;
	}

	void imgui_renderer::end_frame() { }

	void imgui_renderer::invalidate_device_objects()
	{
		m_vertex_buffer.Release();
		m_index_buffer.Release();
		m_vertex_constant_buffer.Release();
		m_pso.Release();
		m_font_srv.Release();
		m_srb.Release();
	}

	void imgui_renderer::invalidate_font_objects()
	{
		m_font_srv.Release();
	}

	void imgui_renderer::create_device_objects(float scale, bool force)
	{
		bool assets_exist  = m_pso;
		bool scale_matches = scale == m_scale;

		if (!force && assets_exist && scale_matches) [[likely]]
		{
			return;
		}

		if (force || !assets_exist)
		{
			invalidate_device_objects();

			ShaderCreateInfo shader_ci;
			shader_ci.UseCombinedTextureSamplers = true;
			shader_ci.SourceLanguage			 = SHADER_SOURCE_LANGUAGE_DEFAULT;

			const auto& deviceCaps = m_device->GetDeviceCaps();

			RefCntAutoPtr<IShader> vs;
			{
				shader_ci.Desc.ShaderType = SHADER_TYPE_VERTEX;
				shader_ci.Desc.Name		  = "Imgui VS";
				switch (deviceCaps.DevType)
				{
				case RENDER_DEVICE_TYPE_VULKAN:
					shader_ci.ByteCode	   = g_vertex_shader_spirv;
					shader_ci.ByteCodeSize = sizeof(g_vertex_shader_spirv);
					break;

				case RENDER_DEVICE_TYPE_D3D11:
				case RENDER_DEVICE_TYPE_D3D12:
					shader_ci.Source = g_vertex_shader_hlsl;
					break;

				case RENDER_DEVICE_TYPE_GL:
				case RENDER_DEVICE_TYPE_GLES:
					shader_ci.Source = g_vertex_shader_glsl;
					break;

				case RENDER_DEVICE_TYPE_METAL:
					shader_ci.Source	 = g_shaders_msl;
					shader_ci.EntryPoint = "vs_main";
					break;

				default:
					UNEXPECTED("Unknown render device type");
				}
				m_device->CreateShader(shader_ci, &vs);
			}

			RefCntAutoPtr<IShader> ps;
			{
				shader_ci.Desc.ShaderType = SHADER_TYPE_PIXEL;
				shader_ci.Desc.Name		  = "Imgui PS";
				switch (deviceCaps.DevType)
				{
				case RENDER_DEVICE_TYPE_VULKAN:
					shader_ci.ByteCode	   = g_fragment_shader_spirv;
					shader_ci.ByteCodeSize = sizeof(g_fragment_shader_spirv);
					break;

				case RENDER_DEVICE_TYPE_D3D11:
				case RENDER_DEVICE_TYPE_D3D12:
					shader_ci.Source = g_pixel_shader_hlsl;
					break;

				case RENDER_DEVICE_TYPE_GL:
				case RENDER_DEVICE_TYPE_GLES:
					shader_ci.Source = g_pixel_shader_glsl;
					break;

				case RENDER_DEVICE_TYPE_METAL:
					shader_ci.Source	 = g_shaders_msl;
					shader_ci.EntryPoint = "ps_main";
					break;

				default:
					UNEXPECTED("Unknown render device type");
				}
				m_device->CreateShader(shader_ci, &ps);
			}

			GraphicsPipelineStateCreateInfo pso_create_info;

			pso_create_info.PSODesc.Name = "ImGUI PSO";
			auto& gfx_pipeline			 = pso_create_info.GraphicsPipeline;

			gfx_pipeline.NumRenderTargets  = 1;
			gfx_pipeline.RTVFormats[0]	   = m_back_buffer_fmt;
			gfx_pipeline.DSVFormat		   = m_depth_buffer_fmt;
			gfx_pipeline.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

			pso_create_info.pVS = vs;
			pso_create_info.pPS = ps;

			gfx_pipeline.RasterizerDesc.CullMode	  = CULL_MODE_NONE;
			gfx_pipeline.RasterizerDesc.ScissorEnable = True;
			gfx_pipeline.DepthStencilDesc.DepthEnable = False;

			auto& rt_0				   = gfx_pipeline.BlendDesc.RenderTargets[0];
			rt_0.BlendEnable		   = True;
			rt_0.SrcBlend			   = BLEND_FACTOR_SRC_ALPHA;
			rt_0.DestBlend			   = BLEND_FACTOR_INV_SRC_ALPHA;
			rt_0.BlendOp			   = BLEND_OPERATION_ADD;
			rt_0.SrcBlendAlpha		   = BLEND_FACTOR_INV_SRC_ALPHA;
			rt_0.DestBlendAlpha		   = BLEND_FACTOR_ZERO;
			rt_0.BlendOpAlpha		   = BLEND_OPERATION_ADD;
			rt_0.RenderTargetWriteMask = COLOR_MASK_ALL;

			LayoutElement vs_inputs[] //
				{
					{0, 0, 2, VT_FLOAT32},	  // pos
					{1, 0, 2, VT_FLOAT32},	  // uv
					{2, 0, 4, VT_UINT8, True} // col
				};
			gfx_pipeline.InputLayout.NumElements	= _countof(vs_inputs);
			gfx_pipeline.InputLayout.LayoutElements = vs_inputs;

			ShaderResourceVariableDesc variables[] = {
				{SHADER_TYPE_PIXEL, "Texture", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC} //
			};
			pso_create_info.PSODesc.ResourceLayout.Variables	= variables;
			pso_create_info.PSODesc.ResourceLayout.NumVariables = _countof(variables);

			SamplerDesc sampler_linear_wrap;
			sampler_linear_wrap.AddressU			  = TEXTURE_ADDRESS_WRAP;
			sampler_linear_wrap.AddressV			  = TEXTURE_ADDRESS_WRAP;
			sampler_linear_wrap.AddressW			  = TEXTURE_ADDRESS_WRAP;
			ImmutableSamplerDesc immutable_samplers[] = {
				{SHADER_TYPE_PIXEL, "Texture", sampler_linear_wrap} //
			};
			pso_create_info.PSODesc.ResourceLayout.ImmutableSamplers	= immutable_samplers;
			pso_create_info.PSODesc.ResourceLayout.NumImmutableSamplers = _countof(immutable_samplers);

			m_device->CreateGraphicsPipelineState(pso_create_info, &m_pso);

			{
				BufferDesc buffer_desc;
				buffer_desc.uiSizeInBytes  = sizeof(float4x4);
				buffer_desc.Usage		   = USAGE_DYNAMIC;
				buffer_desc.BindFlags	   = BIND_UNIFORM_BUFFER;
				buffer_desc.CPUAccessFlags = CPU_ACCESS_WRITE;
				m_device->CreateBuffer(buffer_desc, nullptr, &m_vertex_constant_buffer);
			}
			m_pso->GetStaticVariableByName(SHADER_TYPE_VERTEX, "Constants")->Set(m_vertex_constant_buffer);
		}
		else if (!scale_matches)
		{
			invalidate_font_objects();
		}

		create_fonts_texture(scale);
	}

	void imgui_renderer::create_fonts_texture(float scale)
	{
		// Build texture atlas
		ImGuiIO& io = ImGui::GetIO();

		ImFontConfig cfg;
		cfg.SizePixels = 13 * scale;
		io.Fonts->AddFontDefault(&cfg);

		unsigned char* pixels = nullptr;
		int			   width = 0, height = 0;
		io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

		TextureDesc font_tex_desc;
		font_tex_desc.Name		= "Imgui font texture";
		font_tex_desc.Type		= RESOURCE_DIM_TEX_2D;
		font_tex_desc.Width		= static_cast<Uint32>(width);
		font_tex_desc.Height	= static_cast<Uint32>(height);
		font_tex_desc.Format	= TEX_FORMAT_RGBA8_UNORM;
		font_tex_desc.BindFlags = BIND_SHADER_RESOURCE;
		font_tex_desc.Usage		= USAGE_IMMUTABLE;

		TextureSubResData mip_0_data[] = {{pixels, font_tex_desc.Width * 4}};
		TextureData		  init_data(mip_0_data, _countof(mip_0_data));

		RefCntAutoPtr<ITexture> font_tex;
		m_device->CreateTexture(font_tex_desc, &init_data, &font_tex);
		m_font_srv = font_tex->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);

		m_srb.Release();
		m_pso->CreateShaderResourceBinding(&m_srb, true);
		m_texture_var = m_srb->GetVariableByName(SHADER_TYPE_PIXEL, "Texture");
		VERIFY_EXPR(m_texture_var != nullptr);

		// Store our identifier
		io.Fonts->TexID = (ImTextureID)m_font_srv;

		m_scale = scale;
	}

	float4 imgui_renderer::transform_clip_rect(const ImVec2& display_size, const float4& rect) const
	{
		switch (m_surface_pre_transform)
		{
		case SURFACE_TRANSFORM_IDENTITY:
			return rect;

		case SURFACE_TRANSFORM_ROTATE_90:
		{
			// The image content is rotated 90 degrees clockwise. The origin is in the left-top corner.
			//
			//                                                             DsplSz.y
			//                a.x                                            -a.y     a.y     Old origin
			//              0---->|                                       0------->|<------| /
			//           0__|_____|____________________                0__|________|_______|/
			//            | |     '                    |                | |        '       |
			//        a.y | |     '                    |            a.x | |        '       |
			//           _V_|_ _ _a____b               |               _V_|_ _d'___a'      |
			//            A |     |    |               |                  |   |    |       |
			//  DsplSz.y  | |     |____|               |                  |   |____|       |
			//    -a.y    | |     d    c               |                  |   c'   b'      |
			//           _|_|__________________________|                  |                |
			//              A                                             |                |
			//              |-----> Y'                                    |                |
			//         New Origin                                         |________________|
			//
			float2 a{rect.x, rect.y};
			float2 c{rect.z, rect.w};
			return float4{
				display_size.y - c.y, // min_x = c'.x
				a.x,				  // min_y = a'.y
				display_size.y - a.y, // max_x = a'.x
				c.x					  // max_y = c'.y
			};
		}

		case SURFACE_TRANSFORM_ROTATE_180:
		{
			// The image content is rotated 180 degrees clockwise. The origin is in the left-top corner.
			//
			//                a.x                                               DsplSz.x - a.x
			//              0---->|                                         0------------------>|
			//           0__|_____|____________________                 0_ _|___________________|______
			//            | |     '                    |                  | |                   '      |
			//        a.y | |     '                    |        DsplSz.y  | |              c'___d'     |
			//           _V_|_ _ _a____b               |          -a.y    | |              |    |      |
			//              |     |    |               |                 _V_|_ _ _ _ _ _ _ |____|      |
			//              |     |____|               |                    |              b'   a'     |
			//              |     d    c               |                    |                          |
			//              |__________________________|                    |__________________________|
			//                                         A                                               A
			//                                         |                                               |
			//                                     New Origin                                      Old Origin
			float2 a{rect.x, rect.y};
			float2 c{rect.z, rect.w};
			return float4{
				display_size.x - c.x, // min_x = c'.x
				display_size.y - c.y, // min_y = c'.y
				display_size.x - a.x, // max_x = a'.x
				display_size.y - a.y  // max_y = a'.y
			};
		}

		case SURFACE_TRANSFORM_ROTATE_270:
		{
			// The image content is rotated 270 degrees clockwise. The origin is in the left-top corner.
			//
			//              0  a.x     DsplSz.x-a.x   New Origin              a.y
			//              |---->|<-------------------|                    0----->|
			//          0_ _|_____|____________________V                 0 _|______|_________
			//            | |     '                    |                  | |      '         |
			//            | |     '                    |                  | |      '         |
			//        a.y_V_|_ _ _a____b               |        DsplSz.x  | |      '         |
			//              |     |    |               |          -a.x    | |      '         |
			//              |     |____|               |                  | |      b'___c'   |
			//              |     d    c               |                  | |      |    |    |
			//  DsplSz.y _ _|__________________________|                 _V_|_ _ _ |____|    |
			//                                                              |      a'   d'   |
			//                                                              |                |
			//                                                              |________________|
			//                                                              A
			//                                                              |
			//                                                            Old origin
			float2 a{rect.x, rect.y};
			float2 c{rect.z, rect.w};
			return float4{
				a.y,				  // min_x = a'.x
				display_size.x - c.x, // min_y = c'.y
				c.y,				  // max_x = c'.x
				display_size.x - a.x  // max_y = a'.y
			};
		}

		case SURFACE_TRANSFORM_OPTIMAL:
			UNEXPECTED("SURFACE_TRANSFORM_OPTIMAL is only valid as parameter during swap chain initialization.");
			return rect;

		case SURFACE_TRANSFORM_HORIZONTAL_MIRROR:
		case SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_90:
		case SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_180:
		case SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_270:
			UNEXPECTED("Mirror transforms are not supported");
			return rect;

		default:
			UNEXPECTED("Unknown transform");
			return rect;
		}
	}

	void imgui_renderer::render_draw_data(IDeviceContext* ctx, ImDrawData* draw_data)
	{
		// Avoid rendering when minimized
		if (draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f)
		{
			return;
		}

		// Create and grow vertex/index buffers if needed
		if (!m_vertex_buffer || static_cast<int>(m_vertex_buffer_size) < draw_data->TotalVtxCount)
		{
			m_vertex_buffer.Release();
			while (static_cast<int>(m_vertex_buffer_size) < draw_data->TotalVtxCount)
			{
				m_vertex_buffer_size *= 2;
			}

			BufferDesc vb_desc;
			vb_desc.Name		   = "Imgui vertex buffer";
			vb_desc.BindFlags	   = BIND_VERTEX_BUFFER;
			vb_desc.uiSizeInBytes  = m_vertex_buffer_size * sizeof(ImDrawVert);
			vb_desc.Usage		   = USAGE_DYNAMIC;
			vb_desc.CPUAccessFlags = CPU_ACCESS_WRITE;
			m_device->CreateBuffer(vb_desc, nullptr, &m_vertex_buffer);
		}

		if (!m_index_buffer || static_cast<int>(m_index_buffer_size) < draw_data->TotalIdxCount)
		{
			m_index_buffer.Release();
			while (static_cast<int>(m_index_buffer_size) < draw_data->TotalIdxCount)
			{
				m_index_buffer_size *= 2;
			}

			BufferDesc ib_desc;
			ib_desc.Name		   = "Imgui index buffer";
			ib_desc.BindFlags	   = BIND_INDEX_BUFFER;
			ib_desc.uiSizeInBytes  = m_index_buffer_size * sizeof(ImDrawIdx);
			ib_desc.Usage		   = USAGE_DYNAMIC;
			ib_desc.CPUAccessFlags = CPU_ACCESS_WRITE;
			m_device->CreateBuffer(ib_desc, nullptr, &m_index_buffer);
		}

		{
			MapHelper<ImDrawVert> verts(ctx, m_vertex_buffer, MAP_WRITE, MAP_FLAG_DISCARD);
			MapHelper<ImDrawIdx>  idxs(ctx, m_index_buffer, MAP_WRITE, MAP_FLAG_DISCARD);

			ImDrawVert* vtx_dst = verts;
			ImDrawIdx*	idx_dst = idxs;
			for (int n = 0; n < draw_data->CmdListsCount; n++)
			{
				const ImDrawList* cmd_list = draw_data->CmdLists[n];
				memcpy(vtx_dst, cmd_list->VtxBuffer.Data, cmd_list->VtxBuffer.Size * sizeof(ImDrawVert));
				memcpy(idx_dst, cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx));
				vtx_dst += cmd_list->VtxBuffer.Size;
				idx_dst += cmd_list->IdxBuffer.Size;
			}
		}

		// Setup orthographic projection matrix into our constant buffer
		// Our visible imgui space lies from pDrawData->DisplayPos (top left) to pDrawData->DisplayPos+data_data->DisplaySize (bottom right).
		// DisplayPos is (0,0) for single viewport apps.
		{
			// DisplaySize always refers to the logical dimensions that account for pre-transform, hence
			// the aspect ratio will be correct after applying appropriate rotation.
			float L = draw_data->DisplayPos.x;
			float R = draw_data->DisplayPos.x + draw_data->DisplaySize.x;
			float T = draw_data->DisplayPos.y;
			float B = draw_data->DisplayPos.y + draw_data->DisplaySize.y;

			float4x4 projection{2.0f / (R - L), 0.0f, 0.0f, 0.0f, 0.0f, 2.0f / (T - B), 0.0f, 0.0f, 0.0f, 0.0f, 0.5f, 0.0f, (R + L) / (L - R), (T + B) / (B - T), 0.5f, 1.0f};

			// Bake pre-transform into projection
			switch (m_surface_pre_transform)
			{
			case SURFACE_TRANSFORM_IDENTITY:
				// Nothing to do
				break;

			case SURFACE_TRANSFORM_ROTATE_90:
				// The image content is rotated 90 degrees clockwise.
				projection *= float4x4::RotationZ(-PI_F * 0.5f);
				break;

			case SURFACE_TRANSFORM_ROTATE_180:
				// The image content is rotated 180 degrees clockwise.
				projection *= float4x4::RotationZ(-PI_F * 1.0f);
				break;

			case SURFACE_TRANSFORM_ROTATE_270:
				// The image content is rotated 270 degrees clockwise.
				projection *= float4x4::RotationZ(-PI_F * 1.5f);
				break;

			case SURFACE_TRANSFORM_OPTIMAL:
				UNEXPECTED("SURFACE_TRANSFORM_OPTIMAL is only valid as parameter during swap chain initialization.");
				break;

			case SURFACE_TRANSFORM_HORIZONTAL_MIRROR:
			case SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_90:
			case SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_180:
			case SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_270:
				UNEXPECTED("Mirror transforms are not supported");
				break;

			default:
				UNEXPECTED("Unknown transform");
			}

			MapHelper<float4x4> cb_data(ctx, m_vertex_constant_buffer, MAP_WRITE, MAP_FLAG_DISCARD);
			*cb_data = projection;
		}

		auto setup_render_state = [&]() -> void
		{
			// Setup shader and vertex buffers
			Uint32	 offsets[]		  = {0};
			IBuffer* vertex_buffers[] = {m_vertex_buffer};
			ctx->SetVertexBuffers(0, 1, vertex_buffers, offsets, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);
			ctx->SetIndexBuffer(m_index_buffer, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
			ctx->SetPipelineState(m_pso);

			const float blend_factor[4] = {0.f, 0.f, 0.f, 0.f};
			ctx->SetBlendFactors(blend_factor);

			Viewport vp;
			vp.Width	= static_cast<float>(m_render_surface_width) * draw_data->FramebufferScale.x;
			vp.Height	= static_cast<float>(m_render_surface_height) * draw_data->FramebufferScale.y;
			vp.MinDepth = 0.0f;
			vp.MaxDepth = 1.0f;
			vp.TopLeftX = vp.TopLeftY = 0;
			ctx->SetViewports(
				1,
				&vp,
				static_cast<Uint32>(m_render_surface_width * draw_data->FramebufferScale.x),
				static_cast<Uint32>(m_render_surface_height * draw_data->FramebufferScale.y));
		};

		setup_render_state();

		// Render command lists
		// (Because we merged all buffers into a single one, we maintain our own offset into them)
		int global_idx_offset = 0;
		int global_vtx_offset = 0;

		ITextureView* last_texture_view = nullptr;
		for (int n = 0; n < draw_data->CmdListsCount; n++)
		{
			const ImDrawList* cmd_list = draw_data->CmdLists[n];
			for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++)
			{
				const ImDrawCmd* im_cmd = &cmd_list->CmdBuffer[cmd_i];
				if (im_cmd->UserCallback != NULL)
				{
					// User callback, registered via ImDrawList::AddCallback()
					// (ImDrawCallback_ResetRenderState is a special callback value used by the user to request the renderer to reset render state.)
					if (im_cmd->UserCallback == ImDrawCallback_ResetRenderState)
					{
						setup_render_state();
					}
					else
					{
						im_cmd->UserCallback(cmd_list, im_cmd);
					}
				}
				else
				{
					// Apply scissor/clipping rectangle
					float4 clip_rect{
						(im_cmd->ClipRect.x - draw_data->DisplayPos.x) * draw_data->FramebufferScale.x,
						(im_cmd->ClipRect.y - draw_data->DisplayPos.y) * draw_data->FramebufferScale.y,
						(im_cmd->ClipRect.z - draw_data->DisplayPos.x) * draw_data->FramebufferScale.x,
						(im_cmd->ClipRect.w - draw_data->DisplayPos.y) * draw_data->FramebufferScale.y //
					};
					// Apply pretransform
					clip_rect = transform_clip_rect(draw_data->DisplaySize, clip_rect);

					Rect r{
						static_cast<Int32>(clip_rect.x), static_cast<Int32>(clip_rect.y), static_cast<Int32>(clip_rect.z),
						static_cast<Int32>(clip_rect.w) //
					};
					ctx->SetScissorRects(
						1,
						&r,
						static_cast<Uint32>(m_render_surface_width * draw_data->FramebufferScale.x),
						static_cast<Uint32>(m_render_surface_height * draw_data->FramebufferScale.y));

					// Bind texture
					auto* texture_view = reinterpret_cast<ITextureView*>(im_cmd->TextureId);
					VERIFY_EXPR(texture_view);
					if (texture_view != last_texture_view)
					{
						last_texture_view = texture_view;
						m_texture_var->Set(texture_view);
						ctx->CommitShaderResources(m_srb, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
					}

					// Draw
					DrawIndexedAttribs draw_attribs(im_cmd->ElemCount, sizeof(ImDrawIdx) == 2 ? VT_UINT16 : VT_UINT32, DRAW_FLAG_VERIFY_STATES);
					draw_attribs.FirstIndexLocation = im_cmd->IdxOffset + global_idx_offset;
					draw_attribs.BaseVertex			= im_cmd->VtxOffset + global_vtx_offset;
					ctx->DrawIndexed(draw_attribs);
				}
			}
			global_idx_offset += cmd_list->IdxBuffer.Size;
			global_vtx_offset += cmd_list->VtxBuffer.Size;
		}
	}

} // namespace Diligent
