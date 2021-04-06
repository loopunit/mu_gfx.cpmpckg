#include <mu_gfx.h>

mu::leaf::result<void> update_window(std::shared_ptr<mu_gfx_window>& wnd) noexcept
{
	MU_LEAF_AUTO(wants_to_close, wnd->wants_to_close());
	
	if (!wants_to_close)
	{
		MU_LEAF_AUTO(renderer, mu_gfx()->begin_window(wnd));
		
		MU_LEAF_CHECK(renderer->draw_test());
	}
	else
	{
		wnd.reset();
	}

	return {};
};

int main(int, char**)
{
	return mu::leaf::try_handle_all(
		[]() -> mu::leaf::result<int>
		{
			MU_LEAF_CHECK(mu_gfx()->select_platform());
			MU_LEAF_CHECK(mu_gfx()->init());

			if (auto logger = mu::debug::logger()->stdout_logger())
			{
				logger->info("Hello world");

				auto framework_guard = sg::make_scope_guard(
					[]()
					{
						if (auto res = mu_gfx()->destroy(); !res)
						{
							// report res.error()
						}
					});

				MU_LEAF_AUTO(primary_window, mu_gfx()->open_window(100, 100, 1280, 800));
				MU_LEAF_AUTO(secondary_window, mu_gfx()->open_window(200, 200, 640, 480));
				MU_LEAF_CHECK(primary_window->show());
				MU_LEAF_CHECK(secondary_window->show());

				// Our state
				bool done = false;

				while (!done && (primary_window || secondary_window))
				{
					if (mu_gfx()->pump())
					{
						if (primary_window)
						{
							MU_LEAF_CHECK(update_window(primary_window));
						}
						
						if (secondary_window)
						{
							MU_LEAF_CHECK(update_window(secondary_window));
						}

						MU_LEAF_CHECK(mu_gfx()->present());
					}
					else
					{
						done = true;
					}
				}
			}
			else
			{
			}

			return 0;
		},
		[](mu::leaf::error_info const& unmatched)
		{
			//  "Unknown failure detected" << std::endl <<
			//  "Cryptic diagnostic information follows" << std::endl <<
			//  unmatched;
			return -1;
		});
}
