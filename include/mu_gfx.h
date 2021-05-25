#pragma once

#include <mu_stdlib.h>
#include <mu_stdlib_taskflow.h>

#include <imgui.h>

namespace mu
{
	struct gfx_error
	{
		struct not_specified : common_error
		{
		};
	};

	static inline auto only_gfx_error_handlers = std::make_tuple(
		[](gfx_error::not_specified x, leaf::e_source_location sl)
		{
			debug::logger()->stderr_logger()->log(spdlog::level::err, "{0} :: {1} -> {2} : gfx_error :: not_specified", sl.line, sl.file, sl.function);
		});

	static inline auto gfx_error_handlers = std::tuple_cat(error_handlers, only_gfx_error_handlers);

	template<class TryBlock>
	constexpr inline auto gfx_try_handle(TryBlock&& try_block) -> typename std::decay<decltype(std::declval<TryBlock>()().value())>::type
	{
		return leaf::try_handle_all(try_block, gfx_error_handlers);
	}
} // namespace mu

namespace mu
{
	struct gfx_window : std::enable_shared_from_this<gfx_window>
	{
		std::shared_ptr<gfx_window> get_shared_ptr()
		{
			return shared_from_this();
		}

		gfx_window()		  = default;
		virtual ~gfx_window() = default;

		virtual [[nodiscard]] auto wants_to_close() noexcept -> mu::leaf::result<bool> = 0;
		virtual [[nodiscard]] auto show() noexcept -> mu::leaf::result<void>		   = 0;
		virtual [[nodiscard]] auto begin_frame() noexcept -> mu::leaf::result<void>	   = 0;
		virtual [[nodiscard]] auto begin_imgui() noexcept -> mu::leaf::result<void>	   = 0;
		virtual [[nodiscard]] auto end_imgui() noexcept -> mu::leaf::result<void>	   = 0;
		virtual [[nodiscard]] auto end_frame() noexcept -> mu::leaf::result<void>	   = 0;
		virtual [[nodiscard]] auto make_current() noexcept -> mu::leaf::result<void>   = 0;
	};

	namespace details
	{
		struct gfx_interface
		{
			gfx_interface()			 = default;
			virtual ~gfx_interface() = default;

			virtual [[nodiscard]] auto open_window(int posX, int posY, int sizeX, int sizeY) noexcept -> mu::leaf::result<std::shared_ptr<gfx_window>> = 0;
			virtual [[nodiscard]] auto pump() noexcept -> mu::leaf::result<void>																	   = 0;
			virtual [[nodiscard]] auto present() noexcept -> mu::leaf::result<void>																	   = 0;

			template<typename T_FUNC>
			[[nodiscard]] auto do_frame(T_FUNC func) noexcept -> mu::leaf::result<void>
			{
				MU_LEAF_CHECK(this->pump());
				auto end_frame = gsl::finally(
					[&]() noexcept -> void
					{
						if (auto err = this->present(); !err) [[unlikely]]
						{
							// TODO: log err.error();
						}
					});
				return func();
			}
		};
	} // namespace details

	using gfx = exported_singleton<virtual_singleton<details::gfx_interface>>;
} // namespace mu
