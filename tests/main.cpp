#include <mu_gfx.h>

static auto all_error_handlers = std::tuple_cat(mu::error_handlers, mu::only_gfx_error_handlers);

auto update_window(std::shared_ptr<mu::gfx_window>& wnd) noexcept -> mu::leaf::result<mu::gfx_window::renderer_ref>
{
	if (wnd)
	{
		MU_LEAF_AUTO(wants_to_close, wnd->wants_to_close());

		if (!wants_to_close)
		{
			return wnd->begin_window();
		}
		else
		{
			wnd.reset();
		}
	}

	return mu::gfx_window::renderer_ref{};
};

auto draw_window(std::shared_ptr<mu::gfx_window>& wnd, mu::gfx_window::renderer_ref renderer) noexcept -> mu::leaf::result<void>
{
	MU_LEAF_CHECK(renderer->test());

	return {};
};

auto main(int, char**) -> int
{
	if (
		[]() -> mu::leaf::result<void>
		{
			auto logger = mu::debug::logger()->stdout_logger();
			logger->info("Hello world");

			MU_LEAF_CHECK(mu::gfx()->select_platform());
			MU_LEAF_AUTO(primary_window, mu::gfx()->open_window(100, 100, 1280, 800));
			MU_LEAF_AUTO(secondary_window, mu::gfx()->open_window(200, 200, 640, 480));
			MU_LEAF_CHECK(primary_window->show());
			MU_LEAF_CHECK(secondary_window->show());

			while (primary_window || secondary_window)
			{
				MU_LEAF_AUTO(pumper, mu::gfx()->pump());
				MU_LEAF_AUTO(primary_renderer, update_window(primary_window));
				MU_LEAF_AUTO(secondary_renderer, update_window(secondary_window));

				if (primary_renderer)
				{
					MU_LEAF_CHECK(primary_renderer->test());
				}

				if (secondary_renderer)
				{
					MU_LEAF_CHECK(secondary_renderer->test());
				}
			}
			return {};
		}())
	{
		return 0;
	}
	else
	{
		return mu::leaf::current_error().value();
	}
}
