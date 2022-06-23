//@{"target":{"name":"get_local_extrema.test"}}

#include "./get_local_extrema.hpp"

#include <cmath>
#include <numbers>
#include <algorithm>
#include <cassert>

int main()
{

    {
        std::array<float, 16> vals;
        std::ranges::generate(vals, [k = 0]() mutable {
            auto const phi = static_cast<float>(k*std::numbers::pi_v<float>/8.0f);
            ++k;
            return std::sin(phi);
        });

        auto const vals_out = get_local_extrema<float>(vals);

        assert(std::size(vals_out) == 2);
        assert(vals_out[0].type == extremum_type::max);
        assert(vals_out[1].type == extremum_type::min);
    }

    {
        std::array<float, 2> vals{0.0f, 1.0f};
        auto const vals_out = get_local_extrema<float>(vals);

        assert(std::size(vals_out) == 0);
    }
}
