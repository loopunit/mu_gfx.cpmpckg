#pragma once

#include <mu_stdlib.h>

namespace mu
{
	struct gfx_error
	{
		struct not_specified
		{
		};
	};
} // namespace mu

struct mu_gfx_window
{
	mu_gfx_window()			 = default;
	virtual ~mu_gfx_window() = default;

	virtual mu::leaf::result<bool> wants_to_close() noexcept = 0;
	virtual mu::leaf::result<void> show() noexcept			 = 0;
};

struct mu_gfx_renderer
{
	std::shared_ptr<mu_gfx_window> m_window;
	static void					   release(mu_gfx_renderer* r) noexcept;

	mu::leaf::result<void> draw_test() noexcept;
};

using mu_gfx_renderer_ref = std::unique_ptr<mu_gfx_renderer, decltype(&mu_gfx_renderer::release)>;

struct mu_gfx_interface
{
	mu_gfx_interface()			= default;
	virtual ~mu_gfx_interface() = default;

	virtual mu::leaf::result<void> select_platform() noexcept = 0;
	virtual mu::leaf::result<void> init() noexcept			  = 0;
	virtual mu::leaf::result<void> pump() noexcept			  = 0;
	virtual mu::leaf::result<void> destroy() noexcept		  = 0;

	virtual mu::leaf::result<std::shared_ptr<mu_gfx_window>> open_window(int posX, int posY, int sizeX, int sizeY) noexcept = 0;
	virtual mu::leaf::result<mu_gfx_renderer_ref>			 begin_window(std::shared_ptr<mu_gfx_window> h) noexcept		= 0;
	virtual mu::leaf::result<void>							 present() noexcept												= 0;
};

using mu_gfx = mu::exported_singleton<mu::virtual_singleton<mu_gfx_interface>>;
