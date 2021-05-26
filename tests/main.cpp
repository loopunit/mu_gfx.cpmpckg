#include <mu_gfx.h>

static auto all_error_handlers = std::tuple_cat(mu::error_handlers, mu::only_gfx_error_handlers);

static auto imgui_test_frame(std::shared_ptr<mu::gfx_window>& wwnd, bool& create_new_window) noexcept -> mu::leaf::result<void>
{
	MU_LEAF_CHECK(wwnd->make_current());

	try
	{
		ImGui::PushID(wwnd.get());
		{
			{
				static bool				  demo_open		  = true;
				static bool				  opt_fullscreen  = true;
				static bool				  opt_padding	  = false;
				static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;

				// We are using the ImGuiWindowFlags_NoDocking flag to make the parent window not dockable into,
				// because it would be confusing to have two docking targets within each others.
				ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
				if (opt_fullscreen)
				{
					const ImGuiViewport* viewport = ImGui::GetMainViewport();
					ImGui::SetNextWindowPos(viewport->WorkPos);
					ImGui::SetNextWindowSize(viewport->WorkSize);
					ImGui::SetNextWindowViewport(viewport->ID);
					ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
					ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
					window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
					window_flags |= ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
				}
				else
				{
					dockspace_flags &= ~ImGuiDockNodeFlags_PassthruCentralNode;
				}

				// When using ImGuiDockNodeFlags_PassthruCentralNode, DockSpace() will render our background
				// and handle the pass-thru hole, so we ask Begin() to not render a background.
				if (dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode)
					window_flags |= ImGuiWindowFlags_NoBackground;

				// Important: note that we proceed even if Begin() returns false (aka window is collapsed).
				// This is because we want to keep our DockSpace() active. If a DockSpace() is inactive,
				// all active windows docked into it will lose their parent and become undocked.
				// We cannot preserve the docking relationship between an active window and an inactive docking, otherwise
				// any change of dockspace/settings would lead to windows being stuck in limbo and never being visible.
				if (!opt_padding)
					ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
				ImGui::Begin("DockSpace Demo", &demo_open, window_flags);
				if (!opt_padding)
					ImGui::PopStyleVar();

				if (opt_fullscreen)
					ImGui::PopStyleVar(2);

				// DockSpace
				ImGuiIO& io = ImGui::GetIO();
				if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
				{
					ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
					ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
				}

				if (ImGui::BeginMenuBar())
				{
					if (ImGui::BeginMenu("Options"))
					{
						// Disabling fullscreen would allow the window to be moved to the front of other windows,
						// which we can't undo at the moment without finer window depth/z control.
						ImGui::MenuItem("Fullscreen", NULL, &opt_fullscreen);
						ImGui::MenuItem("Padding", NULL, &opt_padding);
						ImGui::Separator();

						if (ImGui::MenuItem("Flag: NoSplit", "", (dockspace_flags & ImGuiDockNodeFlags_NoSplit) != 0))
						{
							dockspace_flags ^= ImGuiDockNodeFlags_NoSplit;
						}
						if (ImGui::MenuItem("Flag: NoResize", "", (dockspace_flags & ImGuiDockNodeFlags_NoResize) != 0))
						{
							dockspace_flags ^= ImGuiDockNodeFlags_NoResize;
						}
						if (ImGui::MenuItem("Flag: NoDockingInCentralNode", "", (dockspace_flags & ImGuiDockNodeFlags_NoDockingInCentralNode) != 0))
						{
							dockspace_flags ^= ImGuiDockNodeFlags_NoDockingInCentralNode;
						}
						if (ImGui::MenuItem("Flag: AutoHideTabBar", "", (dockspace_flags & ImGuiDockNodeFlags_AutoHideTabBar) != 0))
						{
							dockspace_flags ^= ImGuiDockNodeFlags_AutoHideTabBar;
						}
						if (ImGui::MenuItem("Flag: PassthruCentralNode", "", (dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode) != 0, opt_fullscreen))
						{
							dockspace_flags ^= ImGuiDockNodeFlags_PassthruCentralNode;
						}
						ImGui::Separator();

						if (ImGui::MenuItem("Close", NULL, false, &demo_open != NULL))
							demo_open = false;
						ImGui::EndMenu();
					}
					ImGui::EndMenuBar();
				}

				ImGui::End();
			}
			{
				ImGui::Begin("Hello, world!");

				ImGui::Text("%.3f ms/frame (%.1f FPS)", ImGui::GetIO().DeltaTime * 1000.0f, 1.0f / ImGui::GetIO().DeltaTime);

				create_new_window |= ImGui::Button("Create New Window");

				ImGui::End();
			}
			ImGui::ShowDemoWindow();
		}
		ImGui::PopID();

		return {};
	}
	catch (...)
	{
		return MU_LEAF_NEW_ERROR(mu::gfx_error::not_specified{});
	}
}

struct app_stask_state
{
	std::vector<std::shared_ptr<mu::gfx_window>>& windows;
	bool&										  create_new_window;
	std::array<std::atomic<std::uint32_t>, 8>	  m_window_task_counters;
};

static auto app_test_frame(tf::Executor* executor, app_stask_state* ts) noexcept -> mu::leaf::result<void>
{
	for (auto& cnt : ts->m_window_task_counters)
	{
		cnt = 0;
	}

	tf::Taskflow stage_1;
	for (auto itor = ts->windows.begin(); itor != ts->windows.end(); ++itor)
	{
		stage_1
			.emplace(
				[ts]()
				{
					if (auto func_error = [&]() -> mu::leaf::result<void>
						{
							auto  n	   = ts->m_window_task_counters[0]++;
							auto& wwnd = ts->windows[n];
							MU_LEAF_CHECK(wwnd->begin_frame());
							return {};
						}();
						!func_error) [[unlikely]]
					{
						// log error func_error.get_error_id().value();
					}
				})
			.name("begin_frame");
	}
	executor->run(stage_1).wait();

	for (auto itor = ts->windows.begin(); itor != ts->windows.end(); ++itor)
	{
		{
			auto  n	   = ts->m_window_task_counters[1]++;
			auto& wwnd = ts->windows[n];
			MU_LEAF_CHECK(wwnd->begin_imgui_sync());
		}
		{
			auto  n	   = ts->m_window_task_counters[2]++;
			auto& wwnd = ts->windows[n];
			MU_LEAF_CHECK(wwnd->begin_imgui_async());
		}
		{
			auto  n	   = ts->m_window_task_counters[3]++;
			auto& wwnd = ts->windows[n];
			MU_LEAF_CHECK(imgui_test_frame(wwnd, ts->create_new_window));
		}
	}

	tf::Taskflow stage_2;
	for (auto itor = ts->windows.begin(); itor != ts->windows.end(); ++itor)
	{
		stage_2
			.emplace(
				[ts]()
				{
					if (auto func_error = [&]() -> mu::leaf::result<void>
						{
							{
								auto  n	   = ts->m_window_task_counters[4]++;
								auto& wwnd = ts->windows[n];
								MU_LEAF_CHECK(wwnd->end_imgui_async_1());
							}
							return {};
						}();
						!func_error) [[unlikely]]
					{
						// log error func_error.get_error_id().value();
					}
				})
			.name("begin_end_imgui");
	}
	executor->run(stage_2).wait();

	for (auto itor = ts->windows.begin(); itor != ts->windows.end(); ++itor)
	{
		{
			auto  n	   = ts->m_window_task_counters[5]++;
			auto& wwnd = ts->windows[n];
			MU_LEAF_CHECK(wwnd->end_imgui_sync());
		}
		{
			auto  n	   = ts->m_window_task_counters[6]++;
			auto& wwnd = ts->windows[n];
			MU_LEAF_CHECK(wwnd->end_imgui_async_2());
		}
	}

	tf::Taskflow stage_3;
	for (auto itor = ts->windows.begin(); itor != ts->windows.end(); ++itor)
	{
		stage_3
			.emplace(
				[ts]()
				{
					if (auto func_error = [&]() -> mu::leaf::result<void>
						{
							{
								auto  n	   = ts->m_window_task_counters[7]++;
								auto& wwnd = ts->windows[n];
								MU_LEAF_CHECK(wwnd->end_frame());
							}
							return {};
						}();
						!func_error) [[unlikely]]
					{
						// log error func_error.get_error_id().value();
					}
				})
			.name("end_imgui_end_frame");
	}
	executor->run(stage_3).wait();

	//for (auto itor = ts->windows.begin(); itor != ts->windows.end(); ++itor)
	//{
	//}
	
#if 0
	ts->m_window_task_counter = 0;
	for (auto itor = ts->windows.begin(); itor != ts->windows.end(); ++itor)
	{
		auto  n	   = ts->m_window_task_counter++;
		auto& wwnd = ts->windows[n];
		MU_LEAF_CHECK(wwnd->begin_frame());
	}

	ts->m_window_task_counter = 0;
	for (auto itor = ts->windows.begin(); itor != ts->windows.end(); ++itor)
	{
		auto  n	   = ts->m_window_task_counter++;
		auto& wwnd = ts->windows[n];
		MU_LEAF_CHECK(wwnd->begin_imgui_sync());
	}

	ts->m_window_task_counter = 0;
	for (auto itor = ts->windows.begin(); itor != ts->windows.end(); ++itor)
	{
		auto  n	   = ts->m_window_task_counter++;
		auto& wwnd = ts->windows[n];
		MU_LEAF_CHECK(wwnd->begin_imgui_async());
	}

	ts->m_window_task_counter = 0;
	for (auto itor = ts->windows.begin(); itor != ts->windows.end(); ++itor)
	{
		auto  n	   = ts->m_window_task_counter++;
		auto& wwnd = ts->windows[n];
		MU_LEAF_CHECK(imgui_test_frame(wwnd, ts->create_new_window));
	}

	ts->m_window_task_counter = 0;
	for (auto itor = ts->windows.begin(); itor != ts->windows.end(); ++itor)
	{
		auto  n	   = ts->m_window_task_counter++;
		auto& wwnd = ts->windows[n];
		MU_LEAF_CHECK(wwnd->end_imgui_async_1());
	}

	ts->m_window_task_counter = 0;
	for (auto itor = ts->windows.begin(); itor != ts->windows.end(); ++itor)
	{
		auto  n	   = ts->m_window_task_counter++;
		auto& wwnd = ts->windows[n];
		MU_LEAF_CHECK(wwnd->end_imgui_sync());
	}

	ts->m_window_task_counter = 0;
	for (auto itor = ts->windows.begin(); itor != ts->windows.end(); ++itor)
	{
		auto  n	   = ts->m_window_task_counter++;
		auto& wwnd = ts->windows[n];
		MU_LEAF_CHECK(wwnd->end_imgui_async_2());
	}

	ts->m_window_task_counter = 0;
	for (auto itor = ts->windows.begin(); itor != ts->windows.end(); ++itor)
	{
		auto  n	   = ts->m_window_task_counter++;
		auto& wwnd = ts->windows[n];
		MU_LEAF_CHECK(wwnd->end_frame());
	}
#endif
	return {};
}

auto main(int, char**) -> int
{
	if (auto app_error = []() -> mu::leaf::result<void>
		{
			mu::enable_dpi_awareness();

			auto logger = mu::debug::logger()->stdout_logger();
			logger->info("Hello world");

			std::vector<std::shared_ptr<mu::gfx_window>> windows;
			MU_LEAF_EMPLACE_BACK(windows, mu::gfx()->open_window(100, 100, 1280, 800));

			for (auto& wnd : windows)
			{
				MU_LEAF_CHECK(wnd->show());
			}

			bool create_new_window = false;

			tf::Executor executor;
			while (windows.size() > 0)
			{
				MU_LEAF_CHECK(mu::gfx()->do_frame(
					[&]() noexcept -> mu::leaf::result<void>
					{
						if (create_new_window)
						{
							create_new_window = false;
							if (auto new_wnd_res = mu::gfx()->open_window(200, 200, 640, 480))
							{
								(*new_wnd_res)->show();
								windows.emplace_back(std::move(*new_wnd_res));
							}
						}

						for (auto itor = windows.begin(); itor != windows.end();)
						{
							auto& wwnd = *itor;
							if (wwnd)
							{
								MU_LEAF_AUTO(wants_to_close, wwnd->wants_to_close());

								if (wants_to_close) [[unlikely]]
								{
									itor = windows.erase(itor);
								}
								else
								{
									++itor;
								}
							}
						}

						auto ts = app_stask_state{windows, create_new_window};
						return app_test_frame(&executor, &ts);
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
