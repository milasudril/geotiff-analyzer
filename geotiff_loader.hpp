//@	{"dependencies_extra":[{"ref":"./geotiff_loader.o", "rel":"implementation"},
//@	{"ref":"libtiff-4","origin":"pkg-config"},
//@	{"ref":"geotiff","origin":"system", "rel":"external"}]}

#ifndef GEOTIFF_LOADER_HPP
#define GEOTIFF_LOADER_HPP

#include "./types.hpp"

#include <tiffio.h>
#include <geotiff/xtiffio.h>
#include <geotiff/geotiffio.h>
#include <geotiff/geo_normalize.h>

#include <memory>
#include <variant>
#include <stdexcept>

struct tiff_releaser
{
	void operator()(TIFF* handle) const
	{
		if(handle != nullptr)
		{ XTIFFClose(handle); }
	}
};

std::unique_ptr<TIFF, tiff_releaser> make_tiff(char const* path);

image_size get_image_size(TIFF* handle);

enum class sample_format:int{float_32};

struct strip_info
{
	int32_t rows_per_strip;
};

struct tile_info
{
	vec2u_t sizes;
};

using image_layout = std::variant<strip_info, tile_info>;

image_layout get_image_layout(TIFF* handle);

struct image_info
{
	image_size size;
	int channel_count;
	enum sample_format sample_format;
	image_layout layout;
};

template<class T, class U>
void read(TIFF*, image_size, T*, U const&)
{
	throw std::runtime_error{"Unsupported image format"};
}

void read(TIFF*, image_size, float*, tile_info);

void read(TIFF*, image_size, float*, strip_info);

std::unique_ptr<float[]> load_floats(TIFF* handle, image_info const& img_info);

image_info get_image_info(TIFF* handle);

struct gtif_releaser
{
	void operator()(GTIF* handle) const
	{
		if(handle != nullptr)
		{
			GTIFFree(handle);
		}
	}
};

std::unique_ptr<GTIF, gtif_releaser> make_gtif(TIFF* handle);

inline std::unique_ptr<GTIFDefn> get_defn(GTIF* handle)
{
	auto ret = std::make_unique<GTIFDefn>();
	GTIFGetDefn(handle, ret.get());
	return ret;
}

corners_in_geo_coords get_domain(GTIF* handle, const GTIFDefn& defn, image_size img_size);

#endif