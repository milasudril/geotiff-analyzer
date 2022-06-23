#ifndef TYPES_HPP
#define TYPES_HPP

#include <cstdint>
#include <cstddef>
#include <cmath>
#include <xmmintrin.h>

using vec4_t [[gnu::vector_size(16)]] = float;
using vec2u_t [[gnu::vector_size(16)]] = uint64_t;
using vec2i_t [[gnu::vector_size(16)]] = int64_t;


struct corners_in_geo_coords
{
	vec4_t min;
	vec4_t max;
};

struct image_size
{
	vec2u_t sizes;
};

template<class T>
T& pixel(T* buffer, vec2u_t loc, size_t width)
{
	return buffer[loc[1]*width + loc[0]];
}

inline vec4_t pixel_to_geo_coords(vec2u_t loc,
								  image_size img_size,
								  corners_in_geo_coords const& domain)
{
	auto const loc_norm = vec4_t{static_cast<float>(loc[0]), static_cast<float>(loc[1]), 0.0f, 0.0f}
		/vec4_t{static_cast<float>(img_size.sizes[0]), static_cast<float>(img_size.sizes[1]), 1.0f, 1.0f};

	return loc_norm*domain.max + (vec4_t{1.0f, 1.0f, 0.0f, 0.0f} - loc_norm)*domain.min;
}

inline vec4_t pixel_to_geo_coords(vec4_t loc,
								  image_size img_size,
								  corners_in_geo_coords const& domain)
{
	auto const loc_norm = loc
		/vec4_t{static_cast<float>(img_size.sizes[0]), static_cast<float>(img_size.sizes[1]), 1.0f, 1.0f};

	return loc_norm*domain.max + (vec4_t{1.0f, 1.0f, 0.0f, 0.0f} - loc_norm)*domain.min;
}
inline float dot(vec4_t a, vec4_t b)
{
	a*=b;
	return a[0] + a[1] + a[2] + a[3];
}

inline float norm(vec4_t a)
{
	return std::sqrt(dot(a ,a));
}

inline vec4_t normalized(vec4_t v)
{
	return v/norm(v);
}

inline vec4_t nabla_factors(float R_e, float R_p, vec4_t loc)
{
	auto const λ = loc[0];
	auto const ϕ = loc[1];
	auto const z = loc[2];
	auto const r = R_p/R_e;

	auto const N = R_e/std::sqrt(std::cos(ϕ)*std::cos(ϕ) + r*r*std::sin(ϕ)*std::sin(ϕ));
	auto const dN_dϕ = -R_e*(r*r - 1.0f)*std::cos(ϕ)*std::sin(ϕ)/
		(std::pow(std::cos(ϕ)*std::cos(ϕ) + r*r*std::sin(ϕ)*std::sin(ϕ), 3.0f/2.0f));

	auto const dX_dλ = -(N + z)*std::cos(ϕ)*std::sin(λ);
	auto const dY_dλ =  (N + z)*std::cos(ϕ)*std::cos(λ);
	auto const dZ_dλ =  0.0f;

	auto const dX_dϕ = dN_dϕ * std::cos(ϕ)*std::cos(λ) - (N + z)*std::sin(ϕ)*std::cos(λ);
	auto const dY_dϕ = dN_dϕ * std::cos(ϕ)*std::sin(λ) - (N + z)*std::sin(ϕ)*std::sin(λ);
	auto const dZ_dϕ = r*r * dN_dϕ * std::sin(ϕ) + (r*r*N + z)*std::cos(ϕ);

	return _mm_sqrt_ps(
		vec4_t{
			dX_dλ*dX_dλ + dY_dλ*dY_dλ + dZ_dλ*dZ_dλ,
			dX_dϕ*dX_dϕ + dY_dϕ*dY_dϕ + dZ_dϕ*dZ_dϕ,
			1.0f,
			1.0f
		}
	);
}

#endif