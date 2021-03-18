#include <mu_gfx.h>

int main(int, char**)
{
	return mu::leaf::try_handle_all(

		[]() -> mu::leaf::result<int>
		{
			BOOST_LEAF_CHECK(mu_gfx()->select_platform());

			BOOST_LEAF_CHECK(mu_gfx()->init());

			auto framework_guard = sg::make_scope_guard(
				[]()
				{
					if (auto res = mu_gfx()->destroy(); !res)
					{
						// report error
					}
				});

			// Our state
			bool done = false;

			while (!done)
			{
				BOOST_LEAF_AUTO(pump_result, mu_gfx()->pump());
				if (pump_result)
				{
					BOOST_LEAF_CHECK(mu_gfx()->begin_frame());
					auto frame_guard = sg::make_scope_guard(
						[]()
						{
							if (auto res = mu_gfx()->end_frame(); !res)
							{
								// report error
							}
						});

					mu::leaf::try_catch(
						[&] {},
						[&done](mu::leaf::error_info const& unmatched)
						{
							//  "Unknown failure detected" << std::endl <<
							//  "Cryptic diagnostic information follows" << std::endl <<
							//  unmatched;
							done = true;
						});
				}
				else
				{
					done = true;
				}
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
