#pragma once

#include <mu_stdlib.h>

struct mu_gfx_interface
{
	mu_gfx_interface()			= default;
	virtual ~mu_gfx_interface() = default;

	virtual mu::leaf::result<void> select_platform() noexcept = 0;
	virtual mu::leaf::result<void> init() noexcept			  = 0;
	virtual mu::leaf::result<bool> pump() noexcept			  = 0;
	virtual mu::leaf::result<void> begin_frame() noexcept	  = 0;
	virtual mu::leaf::result<void> end_frame() noexcept		  = 0;
	virtual mu::leaf::result<void> destroy() noexcept		  = 0;
};

using mu_gfx = mu::exported_singleton<mu::virtual_singleton<mu_gfx_interface>>;