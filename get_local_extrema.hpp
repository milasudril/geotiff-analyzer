#ifndef GETPEAKS_HPP
#define GETPEAKS_HPP

#include <span>
#include <vector>
#include <functional>

enum class extremum_type:int{min, max};

template<class T>
struct extremum
{
	T const* item;
	extremum_type type;
};

template<class T, class Compare = std::less<T>>
auto get_local_extrema(std::span<T const> input, Compare cmp = std::less<T>{})
{
	std::vector<extremum<T>> ret;

    if(std::size(input) < 2)
    { return ret; }

	enum class direction : int{up, down};
	auto i = std::data(input);
	auto val_prev = *i;
    ++i;
    auto val = *i;
    auto dir_current = cmp(val, val_prev)? direction::down:direction::up;
    val_prev = val;
	++i;
	while(i != std::data(input) + std::size(input))
	{
		auto const val = *i;

		auto dir = cmp(val, val_prev)? direction::down : direction::up;
		if(dir != dir_current)
		{
			if(dir == direction::up)
			{
				ret.push_back(extremum{i - 1, extremum_type::min});
			}
			else
			{
				ret.push_back(extremum{i - 1, extremum_type::max});
			}
			dir_current = dir;
		}
        val_prev = val;
		++i;
	}
	return ret;
}

#endif
