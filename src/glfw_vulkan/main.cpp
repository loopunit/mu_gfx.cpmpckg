#define NOMINMAX

#include "../mu_gfx_impl.h"

#include <glfw_vulkan/VulkanDevice2.h>
#include <Framework/Vulkan/VulkanSwapchain.h>
#include <framegraph/FG.h>
#include <framegraph/Shared/EnumUtils.h>
#include <pipeline_compiler/VPipelineCompiler.h>

#include <GLFW/glfw3.h>
#ifdef _WIN32
#undef APIENTRY
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h> // for glfwGetWin32Window
#endif

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
		return {};
	}

	virtual mu::leaf::result<void> end_frame() noexcept
	{
		return {};
	}

	virtual mu::leaf::result<void> destroy() noexcept
	{
		destroy_window();

		return {};
	}


	////


	GLFWwindow* m_window = nullptr;


	void show_window()
	{
		glfwShowWindow(m_window);
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
