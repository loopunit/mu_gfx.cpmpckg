#define NOMINMAX

#include "../mu_gfx_impl.h"

#include <glfw_vulkan/VulkanDevice2.h>
#include <Framework/Vulkan/VulkanSwapchain.h>
#include <framegraph/FG.h>
#include <framegraph/Shared/EnumUtils.h>
#include <pipeline_compiler/VPipelineCompiler.h>

//#include <array>
//#include <limits>
//#include <memory>
//
//#include <GLFW/glfw3.h>
//#ifdef _WIN32
//#undef APIENTRY
//#define GLFW_EXPOSE_NATIVE_WIN32
//#include <GLFW/glfw3native.h> // for glfwGetWin32Window
//#endif
//
//#if defined(_WIN32) && !defined(IMGUI_DISABLE_WIN32_FUNCTIONS) && !defined(IMGUI_DISABLE_WIN32_DEFAULT_IME_FUNCTIONS) && !defined(__GNUC__)
//#define HAS_WIN32_IME 1
//#include <imm.h>
//#ifdef _MSC_VER
//#pragma comment(lib, "imm32")
//#endif
//#else
//#define HAS_WIN32_IME 0
//#endif
//
//#include <fstream>
//#include <filesystem>
//#include <map>

struct mu_gfx_impl : public mu_gfx_interface
{
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
		// BOOST_LEAF_CHECK(init({100.0f, 100.0f}, {1280.0f, 800.0f}));
		return {};
	}

	virtual mu::leaf::result<bool> pump() noexcept
	{
		// glfwPollEvents();
		// return !glfwWindowShouldClose(m_window);
		return true;
	}

	virtual mu::leaf::result<void> begin_frame() noexcept
	{
		// new_frame();
		//
		// if (ImGuiViewport* main_viewport = ImGui::GetMainViewport(); main_viewport->PlatformRequestResize)
		//{
		//	((platform_renderer_data*)main_viewport->RendererUserData)->handle_resize(main_viewport);
		//	main_viewport->PlatformRequestResize = false;
		//}
		//
		// ImGui::NewFrame();

		return {};
	}

	virtual mu::leaf::result<void> end_frame() noexcept
	{
		// ImGui::Render();
		//
		// ImGuiViewport*			main_viewport	   = ImGui::GetMainViewport();
		// platform_renderer_data* main_viewport_data = (platform_renderer_data*)main_viewport->RendererUserData;
		//
		// if (main_viewport->PlatformRequestResize)
		//{
		//	main_viewport_data->handle_resize(main_viewport);
		//	main_viewport->PlatformRequestResize = false;
		//}
		//
		// if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		//{
		//	ImGui::UpdatePlatformWindows();
		//}
		//
		// m_pending_task = main_viewport_data->load_assets(m_context);
		// main_viewport_data->render_frame(m_context, main_viewport, ImGui::GetDrawData(), m_pending_task);
		//
		// if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		//{
		//	ImGui::RenderPlatformWindowsDefault(nullptr, nullptr);
		//}
		// main_viewport_data->end_frame();

		return {};
	}

	virtual mu::leaf::result<void> destroy() noexcept
	{
		// shutdown_renderer();
		// shutdown_window();
		// ImGui::DestroyContext();
		// m_context = nullptr;
		//
		// destroy_window();

		return {};
	}
};

///

MU_DEFINE_VIRTUAL_SINGLETON(mu_gfx_interface, mu_gfx_impl);
MU_EXPORT_SINGLETON(mu_gfx);
