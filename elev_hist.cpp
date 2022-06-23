//@	{"target":{"name":"elev_hist.o"}}

#include "./geotiff_loader.hpp"
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

struct rgb8
{
	uint8_t r;
	uint8_t g;
	uint8_t b;
};

void dump(vec4_t const* pixels, image_size size, char const* filename)
{
	auto const range = std::span{pixels, size.sizes[0]*size.sizes[1]};
	auto const minmax_r = std::ranges::minmax_element(range, [](auto a, auto b){
		return a[0] < b[0];
	});
	auto const minmax_g = std::ranges::minmax_element(range, [](auto a, auto b){
		return a[1] < b[1];
	});
	auto const minmax_b = std::ranges::minmax_element(range, [](auto a, auto b){
		return a[2] < b[2];
	});

	vec4_t const min_vals{(*minmax_r.min)[0], (*minmax_g.min)[1], (*minmax_b.min)[2], 0.0f};
	vec4_t const max_vals{(*minmax_r.max)[0], (*minmax_g.max)[1], (*minmax_b.max)[2], 1.0f};

	printf("min: %.8g %.8g %.8g\n", min_vals[0], min_vals[1], min_vals[2]);
	printf("max: %.8g %.8g %.8g\n", max_vals[0], max_vals[1], max_vals[2]);

	auto temp = std::make_unique<rgb8[]>(std::size(range));

	std::ranges::transform(range, temp.get(), [min_vals, max_vals](auto val){
		auto const rescaled = 255.0f*(val - min_vals)/(max_vals - min_vals);
		return rgb8{static_cast<uint8_t>(rescaled[0]),
			static_cast<uint8_t>(rescaled[1]),
			static_cast<uint8_t>(rescaled[2])
		};
	});

	auto dest = std::unique_ptr<FILE, decltype(&fclose)>{fopen(filename, "wb"), fclose};
	fwrite(temp.get(), std::size(range), sizeof(rgb8), dest.get());
}

auto get_pair(std::string_view arg_val)
{
	auto const i = std::ranges::find(arg_val, ',');
	if(i == std::end(arg_val))
	{
		return std::pair{std::string{std::begin(arg_val)}, std::optional<std::string>{}};
	}
	return std::pair{std::string{std::begin(arg_val), i}, std::optional{std::string{i + 1, std::end(arg_val)}}};
}

int main(int argc, char** argv)
{
	if(argc < 1)
	{
		return -1;
	}

	constexpr auto bucket_size = 32.0f;
	constexpr size_t N = 8900/bucket_size;
	std::array<double, N> histogram{};

	for(auto item : std::span{argv + 1, static_cast<size_t>(argc - 1)})
	{
		auto [heightmap_name, mask_name] = get_pair(item);
		auto tiff = make_tiff(heightmap_name.c_str());
		auto gtif = make_gtif(tiff.get());

		auto const info = get_image_info(tiff.get());
		auto defn = get_defn(gtif.get());
		auto const domain = get_domain(gtif.get(), *defn, info.size);
		auto const R_e = static_cast<float>(defn->SemiMajor);
		auto const R_p = static_cast<float>(defn->SemiMinor);
		fprintf(stderr, "domain: min=(%.7g, %.7g), max=(%.7g, %.7g), R_e=%.8g, R_p=%.8g\n",
			domain.min[0], domain.min[1], domain.max[0], domain.max[1],
			R_e, R_p);
		putc('\n', stderr);

		auto pixels = load_floats(tiff.get(), info);
		auto src_ptr = static_cast<float const*>(pixels.get());

		auto mask = get_or(get_or(mask_name, file{}, "rb"), blob<uint8_t>{}, info.size.sizes[0]*info.size.sizes[1]);

		auto const longlat_delta = pixel_to_geo_coords(vec2u_t{1, 0}, info.size, domain)
			- pixel_to_geo_coords(vec2u_t{0, 1}, info.size, domain);

		vec2u_t loc{0, 0};
		for(;loc[1] != info.size.sizes[1]; loc+=vec2u_t{0, 1})
		{
			for(loc[0] = 1; loc[0] != info.size.sizes[0]; loc+=vec2u_t{1, 0})
			{
				auto const w = info.size.sizes[0];
				if(mask.get() == nullptr || pixel(mask.get(), loc, w))
				{
					auto const val = pixel(src_ptr, loc, w);

					if(val > 1.0f)
					{
						auto const loc_long_lat = pixel_to_geo_coords(loc, info.size, domain);
						auto const scale_factors = nabla_factors(R_e, R_p, loc_long_lat)*longlat_delta;
						auto const bucket = static_cast<size_t>(val/bucket_size);
 						histogram[bucket] += scale_factors[0]*scale_factors[1];
					}
				}
			}
		}
	}

	std::ranges::for_each(histogram, [bucket = 0](auto const& item) mutable {
		auto const z0 = bucket_size*bucket;
		auto const z1 = bucket_size*(bucket + 1);

		printf("%.8e %.16g\n", 0.5f*(z0 + z1), item/(z1 - z0));
		++bucket;
	});
}