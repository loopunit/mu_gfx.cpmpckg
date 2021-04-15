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

			std::vector<std::shared_ptr<mu::gfx_window>> windows;
			MU_LEAF_EMPLACE_BACK(windows, mu::gfx()->open_window(100, 100, 1280, 800));
			MU_LEAF_EMPLACE_BACK(windows, mu::gfx()->open_window(200, 200, 640, 480));

			for (auto& wnd : windows)
			{
				MU_LEAF_CHECK(wnd->show());
			}

			while (
				[&]() noexcept -> bool
				{
					for (auto& wnd : windows)
					{
						if (wnd)
						{
							return true;
						}
					}
					return false;
				}())
			{
				MU_LEAF_AUTO(pumper, mu::gfx()->pump());

				for (auto& wwnd : windows)
				{
					if (wwnd)
					{
						MU_LEAF_AUTO(wants_to_close, wwnd->wants_to_close());

						if (!wants_to_close) [[likely]]
						{
							if (auto wnd = wwnd->begin_window(); wnd) [[likely]]
							{
								if (auto& r = *wnd; r) [[likely]]
								{
									MU_LEAF_CHECK(r->test());
								}
							}
						}
						else
						{
							wwnd.reset();
						}
					}
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
