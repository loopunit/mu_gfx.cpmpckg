#include <mu_gfx.h>

static auto all_error_handlers = std::tuple_cat(mu::error_handlers, mu::only_gfx_error_handlers);

auto main(int, char**) -> int
{
	if (auto app_error = []() -> mu::leaf::result<void>
		{
			auto logger = mu::debug::logger()->stdout_logger();
			logger->info("Hello world");

			std::vector<std::shared_ptr<mu::gfx_window>> windows;
			MU_LEAF_EMPLACE_BACK(windows, mu::gfx()->open_window(100, 100, 1280, 800));
			MU_LEAF_EMPLACE_BACK(windows, mu::gfx()->open_window(200, 200, 640, 480));

			for (auto& wnd : windows)
			{
				MU_LEAF_CHECK(wnd->show());
			}

			while (windows.size() > 0)
			{
				MU_LEAF_CHECK(mu::gfx()->do_frame(
					[&]() noexcept -> mu::leaf::result<void>
					{
						for (auto itor = windows.begin(); itor != windows.end();)
						{
							auto& wwnd = *itor;
							if (wwnd)
							{
								MU_LEAF_AUTO(wants_to_close, wwnd->wants_to_close());

								if (!wants_to_close) [[likely]]
								{
									MU_LEAF_CHECK(wwnd->do_frame(
										[&wwnd]() -> mu::leaf::result<void>
										{
											return wwnd->do_imgui(
												[]() -> mu::leaf::result<void>
												{
													try
													{
														ImGui::ShowDemoWindow();
														return {};
													}
													catch (...)
													{
														return MU_LEAF_NEW_ERROR(mu::gfx_error::not_specified{});
													}
												});
										}));
									++itor;
								}
								else
								{
									itor = windows.erase(itor);
								}
							}
						}
						return {};
					}));
			}
			return {};
		}();
		!app_error) [[unlikely]]
	{
		return app_error.get_error_id().value();
	}

	return 0;
}
