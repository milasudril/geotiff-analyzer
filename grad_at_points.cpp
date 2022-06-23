//@	{"target":{"name":"grad_at_points.o"}}

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

	std::array<std::vector<std::tuple<float, float>>, 159> histogram;

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

		vec2u_t loc{1, 1};
		for(;loc[1] != info.size.sizes[1] - 1; loc+=vec2u_t{0, 1})
		{
			for(loc[0] = 1; loc[0] != info.size.sizes[0] - 1; loc+=vec2u_t{1, 0})
			{
				auto const w = info.size.sizes[0];
				if(mask.get() == nullptr || pixel(mask.get(), loc, w))
				{
					auto const val11 = pixel(src_ptr, loc, w);

					auto const val10 = pixel(src_ptr, loc-vec2u_t{0, 1}, w);
					auto const val01 = pixel(src_ptr, loc-vec2u_t{1, 0}, w);
					auto const val21 = pixel(src_ptr, loc+vec2u_t{1, 0}, w);
					auto const val12 = pixel(src_ptr, loc+vec2u_t{0, 1}, w);

					auto const loc10 = pixel_to_geo_coords(loc-vec2u_t{0, 1}, info.size, domain);
					auto const loc01 = pixel_to_geo_coords(loc-vec2u_t{1, 0}, info.size, domain);
					auto const loc11 = pixel_to_geo_coords(loc, info.size, domain);
					auto const loc21 = pixel_to_geo_coords(loc+vec2u_t{1, 0}, info.size, domain);
					auto const loc12 = pixel_to_geo_coords(loc+vec2u_t{0, 1}, info.size, domain);


					auto const dz_dλ = (val21 - val01)/(loc21[0] - loc01[0]);
					auto const dz_dϕ = (val12 - val10)/(loc12[1] - loc10[1]);

					auto const scale_factors = 1.0f/nabla_factors(R_e, R_p, loc11 + vec4_t{0.0f, 0.0f, val11, 0.0f});

					auto const grad = norm(scale_factors*vec4_t{dz_dλ, dz_dϕ, 0.0f, 0.0f});

					if(val11 > 1.0f && grad > 1.0f/2048.0f)
					{
						auto const bucket = static_cast<size_t>(val11 < 1.0f ? 0.0f : 12.0f*std::log2(val11));
						histogram[bucket].push_back(std::tuple{val11, grad});
					}
				}
			}
		}
	}

	std::mt19937 rng;
	std::ranges::for_each(histogram, [&rng, bucket = 0](auto& item) mutable {
		std::shuffle(std::begin(item), std::end(item), rng);
		auto const N = 1024;
		auto ptr = std::begin(item);
		size_t k = 0;
		while(k != N && ptr != std::end(item))
		{
			printf("%.8g %.8g\n",
				   std::get<0>(*ptr),
				   std::get<1>(*ptr));
			++k;
			++ptr;
		}
		++bucket;
	});
}