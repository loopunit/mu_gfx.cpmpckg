#pragma once

#include <mu_stdlib.h>

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
	struct gfx_renderer
	{
		gfx_renderer()			= default;
		virtual ~gfx_renderer() = default;

		virtual auto test() noexcept -> mu::leaf::result<void>		= 0;
		virtual auto end_frame() noexcept -> mu::leaf::result<void> = 0;
	};

	struct gfx_window : public std::enable_shared_from_this<gfx_window>
	{
		using renderer_ref = std::unique_ptr<gfx_renderer, std::function<void(gfx_renderer*)>>;

		gfx_window()		  = default;
		virtual ~gfx_window() = default;

		virtual auto wants_to_close() noexcept -> mu::leaf::result<bool>	   = 0;
		virtual auto show() noexcept -> mu::leaf::result<void>				   = 0;
		virtual auto begin_window() noexcept -> mu::leaf::result<renderer_ref> = 0;

	protected:
		std::shared_ptr<gfx_window> shared_self() noexcept
		{
			return shared_from_this();
		}
	};

	struct gfx_pumper
	{
		gfx_pumper()		  = default;
		virtual ~gfx_pumper() = default;

		virtual auto present() noexcept -> mu::leaf::result<void> = 0;
	};

	namespace details
	{
		struct gfx_interface
		{
			gfx_interface()			 = default;
			virtual ~gfx_interface() = default;

			using pumper_ref = std::unique_ptr<gfx_pumper, std::function<void(gfx_pumper*)>>;

			virtual auto select_platform() noexcept -> leaf::result<bool>																 = 0;
			virtual auto open_window(int posX, int posY, int sizeX, int sizeY) noexcept -> mu::leaf::result<std::shared_ptr<gfx_window>> = 0;
			virtual auto pump() noexcept -> mu::leaf::result<pumper_ref>																 = 0;
			virtual auto present() noexcept -> mu::leaf::result<void>																	 = 0;
		};
	} // namespace details

	using gfx = exported_singleton<virtual_singleton<details::gfx_interface>>;
} // namespace mu
