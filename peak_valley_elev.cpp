//@	{"target":{"name":"peak_valley_elev.o"}}

#include "./geotiff_loader.hpp"
#include "./get_local_extrema.hpp"
#include "./file.hpp"
#include "./blob.hpp"
#include "./cmdline.hpp"

#include <cmath>
#include <array>
#include <stdexcept>
#include <memory>
#include <numbers>
#include <algorithm>
#include <span>
#include <random>
#include <cassert>

auto get_pair(std::string_view arg_val)
{
	auto const i = std::ranges::find(arg_val, ',');
	if(i == std::end(arg_val))
	{
		return std::pair{std::string{std::begin(arg_val)}, std::optional<std::string>{}};
	}
	return std::pair{std::string{std::begin(arg_val), i}, std::optional{std::string{i + 1, std::end(arg_val)}}};
}

vec4_t get_origin(uint8_t const* pixels, image_size size, std::mt19937& rng)
{
	if(size.sizes[0] < 3 || size.sizes[1] < 3)
	{ throw std::runtime_error{"Too small domain"};}

	std::uniform_int_distribution ux{static_cast<size_t>(1), size.sizes[0] - 1};
	std::uniform_int_distribution uy{static_cast<size_t>(1), size.sizes[1] - 1};

	while(true)
	{
		auto const x = ux(rng);
		auto const y = uy(rng);

		if(pixel(pixels, vec2u_t{x, y}, size.sizes[0]))
		{
			return vec4_t{static_cast<float>(x), static_cast<float>(y), 0.0f, 0.0f};
		}
	}
}

vec4_t get_direction(std::mt19937& rng)
{
    auto const angle=std::move(std::uniform_real_distribution{0.0f, std::numbers::pi_v<float>})(rng);
	return vec4_t{std::cos(angle), std::sin(angle), 0.0f, 0.0f};
}

vec4_t get_start_loc(vec4_t origin, vec4_t dir, float w)
{
	// find axis intersections from the equation
	// origin - r*dir = 0

	auto const r1 = origin[0]/dir[0];
	auto const r2 = origin[1]/dir[1];

	// r1 might be negative
	if(r1 < 0.0f)
	{
		// but it must be positive for the solution to be correct.
		// Thus r1 is discarded. Use width to compute a new one.
		auto const r1_alt = (origin[0] - w)/dir[0];
		return origin - dir*std::min(r2, r1_alt);
	}
	else
	{
		// If both r1 and r2 are positive, choose the smaller to stay within boundaries
		return origin - dir*std::min(r1, r2);
	}
}

template<class T>
float interp(T const* img, vec4_t loc, image_size size)
{
	auto const w = size.sizes[0];
	auto const h = size.sizes[1];
	auto const x_0  = (static_cast<uint32_t>(loc[0]) + w) % w;
	auto const y_0  = (static_cast<uint32_t>(loc[1]) + h) % h;
	auto const x_1  = (x_0 + w + 1) % w;
	auto const y_1  = (y_0 + h + 1) % h;

	auto const z_00 = pixel(img, vec2u_t{x_0, y_0}, w);
	auto const z_01 = pixel(img, vec2u_t{x_0, y_1}, w);
	auto const z_10 = pixel(img, vec2u_t{x_1, y_0}, w);
	auto const z_11 = pixel(img, vec2u_t{x_1, y_1}, w);

	auto const xi = loc - vec4_t{static_cast<float>(x_0), static_cast<float>(y_0), 0.0f, 0.0f};

	auto const z_x0 = (1.0f - static_cast<float>(xi[0])) * z_00 + static_cast<float>(xi[0]) * z_10;
	auto const z_x1 = (1.0f - static_cast<float>(xi[0])) * z_01 + static_cast<float>(xi[0]) * z_11;
	return (1.0f - static_cast<float>(xi[1])) * z_x0 + static_cast<float>(xi[1]) * z_x1;
}

struct ray
{
	vec4_t origin;
	vec4_t direction;
};

using curve = std::vector<vec4_t>;

std::vector<curve> get_cross_section(ray r,
                                     float const* heightmap,
                                     uint8_t const* mask,
                                     image_size size,
									 corners_in_geo_coords const& domain,
									 float R_e,
									 float R_p)
{
	std::vector<curve> ret;
	auto loc_prev = r.origin;
	auto loc = loc_prev + r.direction;
	float t = 0.0f;

	curve current;

	while(loc[0]>=0.0f && loc[0] < static_cast<float>(size.sizes[0]) &&
		loc[1] < static_cast<float>(size.sizes[1]))
	{
		auto const mask_val = interp(mask, loc, size);
		auto const z = interp(heightmap, loc, size);
		if(mask_val < 0.5f || z < 1.0f)
		{
			if(std::size(current) != 0)
			{
				ret.push_back(std::move(current));
				t = 0.0f;
			}
		}
		else
		{
			assert(z < 9000.0f);
			current.push_back(vec4_t{t, z, 0.0f, 0.0f});
			auto const lambda_phi = pixel_to_geo_coords(loc, size, domain);
			auto const longlat_delta = lambda_phi - pixel_to_geo_coords(loc_prev, size, domain);
			auto const delta = nabla_factors(R_e, R_p, lambda_phi)*longlat_delta;
			t += std::sqrt(delta[0]*delta[0] + delta[1]*delta[1]);
		}
		loc_prev = loc;
		loc+=r.direction;
	}

	if(std::size(current) != 0)
	{
		ret.push_back(std::move(current));
	}

//	assert(std::size(ret) != 0);
	return ret;
}

curve filter(std::span<vec4_t const> vals)
{
	auto const f = 2.0f*std::numbers::pi_v<float>*1.0f/2048.0f;
	curve ret;
	std::ranges::transform(vals, std::back_inserter(ret), [z = 0.0f, t = 0.0f, f](auto val) mutable {
		auto const dt = val[0] - t;
		z += f*dt*(val[1] - z);
		t = val[0];
		return vec4_t{t, z, 0.0f, 0.0f};
	});

	return ret;
}

int main(int argc, char** argv)
{
	if(argc < 1)
	{
		return -1;
	}

	std::mt19937 rng;

	struct peak_data
	{
		float t;
		float min;
		float max;
	};

	std::array<std::vector<peak_data>, 159> histogram;

	for(auto item : std::span{argv + 1, static_cast<size_t>(argc - 1)})
	{
		auto [heightmap_name, mask_name] = get_pair(item);
		auto tiff = make_tiff(heightmap_name.c_str());
		auto gtif = make_gtif(tiff.get());

		auto const info = get_image_info(tiff.get());
		auto defn = get_defn(gtif.get());
		auto const domain = get_domain(gtif.get(), *defn, info.size);
		if(info.size.sizes[0] == 0)
		{ throw std::runtime_error{"Invalid size"};}
		auto const R_e = static_cast<float>(defn->SemiMajor);
		auto const R_p = static_cast<float>(defn->SemiMinor);
		fprintf(stderr, "domain: min=(%.7g, %.7g), max=(%.7g, %.7g), R_e=%.8g, R_p=%.8g\n",
			domain.min[0], domain.min[1], domain.max[0], domain.max[1],
			R_e, R_p);
		putc('\n', stderr);

		auto heightmap = load_floats(tiff.get(), info);
		auto mask = get_or(get_or(mask_name, file{}, "rb"), blob<uint8_t>{}, info.size.sizes[0]*info.size.sizes[1]);

		auto const pixel_count = std::count_if(mask.get(), mask.get() + info.size.sizes[0]*info.size.sizes[1],
											   [](auto const val) { return val != 0; });
		fprintf(stderr, "pixel_count: %zu\n", pixel_count);
		auto const N = (8lu * 65536lu * 8192lu)/static_cast<size_t>(std::sqrt(pixel_count));
		fprintf(stderr, "N: %zu\n", N);
		for(size_t k = 0; k != N; ++k)
		{
			auto const origin = get_origin(mask.get(), info.size, rng);
			ray r{};
			r.direction = get_direction(rng);
			r.origin = get_start_loc(origin, r.direction, static_cast<float>(info.size.sizes[0] - 1));
			auto const curves = get_cross_section(r, heightmap.get(), mask.get(), info.size, domain, R_e, R_p);

			std::ranges::for_each(curves, [&histogram](auto const& val) {
				auto const filtered_val = filter(val);
				auto const extrema = get_local_extrema<vec4_t>(filtered_val, [](auto a, auto b){ return a[1] < b[1]; });
				if(std::size(extrema) < 3)
				{ return; }

				auto next_peak = [vals_end = std::end(extrema) - 1](auto start) {
					return std::find_if(start + 1, vals_end, [](auto const& item) {
						return item.type == extremum_type::max;
					});
				};

				auto i_peak = next_peak(std::begin(extrema));

				while(i_peak != std::end(extrema) - 1)
				{
					auto const peak = *(i_peak->item);
					auto const i_valley_a = i_peak - 1;
					auto const i_valley_b = i_peak + 1;

					auto const valley_a = *(i_valley_a->item);
					auto const valley_b = *(i_valley_b->item);

					auto const t_peak = peak[0];
					auto const t_valley_a = valley_a[0];
					auto const t_valley_b = valley_b[0];

					auto const dt = t_valley_b - t_valley_a;
					auto const xi = (t_peak - t_valley_a)/dt;

					auto const t = t_peak;
					auto const min = xi*valley_b[1] + (1.0f - xi)*valley_a[1];
					auto const max = peak[1];
					auto const bucket = static_cast<size_t>(max < 1.0f ? 0.0f : 12.0f*std::log2(max));
					if(max - min > 32.0f)
					{ histogram[bucket].push_back(peak_data{t, min, max}); }
					i_peak = next_peak(i_peak);
				 }
			});
		}
	}

	std::ranges::for_each(histogram, [&rng, bucket = 0](auto& item) mutable {
		std::shuffle(std::begin(item), std::end(item), rng);
		auto const N = 1024;
		auto ptr = std::begin(item);
		size_t k = 0;
		while(k != N && ptr != std::end(item))
		{
			printf("%.8g %.8g\n", ptr->min, ptr->max);
			++k;
			++ptr;
		}
		++bucket;
	});
}
