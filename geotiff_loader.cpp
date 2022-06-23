//@	{"target":{"name":"geotiff_loader.o"}}

#include "./geotiff_loader.hpp"

#include <numbers>

std::unique_ptr<TIFF, tiff_releaser> make_tiff(char const* path)
{
	std::unique_ptr<TIFF, tiff_releaser> ret{XTIFFOpen(path, "r")};
	if(ret.get() == nullptr)
	{
		throw std::runtime_error{"Failed to open file"};
	}
	return ret;
}

image_size get_image_size(TIFF* handle)
{
	int width{};
	int height{};
	TIFFGetField(handle, TIFFTAG_IMAGEWIDTH, &width );
	TIFFGetField(handle, TIFFTAG_IMAGELENGTH, &height );

	image_size ret{};
	ret.sizes = vec2u_t{static_cast<size_t>(width), static_cast<size_t>(height)};
	return ret;
}

void read(TIFF* handle, image_size image_size, float* buffer, tile_info tile_info)
{
	auto tile_buffer = std::make_unique<float[]>(tile_info.sizes[0]*tile_info.sizes[1]);

	// Subtract, because true is -1 in gcc vector math
	auto const tile_count = image_size.sizes/tile_info.sizes - (image_size.sizes%tile_info.sizes != 0);
	vec2u_t loc{0, 0};
	for(;loc[1] != tile_count[1]; loc+=vec2u_t{0, 1})
	{
		for(loc[0] = 0; loc[0] != tile_count[0]; loc+=vec2u_t{1, 0})
		{
			auto const src_loc = loc*tile_info.sizes;
			TIFFReadTile(handle, tile_buffer.get(), src_loc[0], src_loc[1], 0, 0);
			auto const rem = image_size.sizes - src_loc;
			auto const pixels_to_read = rem < tile_info.sizes?rem:tile_info.sizes;

			vec2u_t tile_loc{0, 0};
			for(;tile_loc[1] != pixels_to_read[1]; tile_loc+=vec2u_t{0, 1})
			{
				for(tile_loc[0] = 0;tile_loc[0] != pixels_to_read[0]; tile_loc+=vec2u_t{1, 0})
				{
					auto const dest_loc = tile_loc + src_loc;
					pixel(buffer, dest_loc, image_size.sizes[0])
					  = pixel(tile_buffer.get(), tile_loc, tile_info.sizes[0]);
				}
			}
		}
	}
}

void read(TIFF* handle, image_size image_size, float* buffer, strip_info)
{
	for(size_t k = 0; k != image_size.sizes[1]; ++k)
	{
		TIFFReadScanline(handle, buffer + k*image_size.sizes[0], k);
	}
}

std::unique_ptr<float[]> load_floats(TIFF* handle, image_info const& img_info)
{
	auto const size = (static_cast<size_t>(img_info.size.sizes[0]) * static_cast<size_t>(img_info.size.sizes[1]));
	auto ret = std::make_unique<float[]>(size);

	std::visit([handle, &img_info, output_ptr = ret.get()](auto const& layout){
		read(handle, img_info.size, output_ptr, layout);
	}, img_info.layout);

	return ret;
}

image_layout get_image_layout(TIFF* handle)
{
	auto const rows_per_strip = [](auto handle){
		int val{};
		if(!TIFFGetField(handle, TIFFTAG_ROWSPERSTRIP, &val))
		{ return std::optional<int>{}; }
		return std::optional{val};
	}(handle);

	auto const tile_width = [](auto handle) {
		int val{};
		if(!TIFFGetField(handle, TIFFTAG_TILEWIDTH, &val))
		{ return std::optional<size_t>{}; }
		return std::optional{static_cast<size_t>(val)};
	}(handle);

	auto const tile_height = [](auto handle){
				int val{};
		if(!TIFFGetField(handle, TIFFTAG_TILELENGTH, &val))
		{ return std::optional<size_t>{}; }
		return std::optional{static_cast<size_t>(val)};
	}(handle);

	if(rows_per_strip.has_value() && (tile_width.has_value() || tile_height.has_value()))
	{ throw std::runtime_error{"Ambigous image layout"}; }

	if(rows_per_strip.has_value())
	{
		return image_layout{strip_info{*rows_per_strip}};
	}

	if(tile_width.has_value() && tile_height.has_value())
	{
		return image_layout{tile_info{*tile_width, *tile_height}};
	}

	throw std::runtime_error{"Unsupported or unknown image layout"};
}

image_info get_image_info(TIFF* handle)
{
	image_info ret;
	ret.size = get_image_size(handle);

	uint16_t bits_per_sample{};
	if(!TIFFGetField(handle, TIFFTAG_BITSPERSAMPLE, &bits_per_sample))
	{
		throw std::runtime_error{"Unknown number of bits per sample"};
	}

	uint16_t sample_format{};
	if(!TIFFGetField(handle, TIFFTAG_DATATYPE, &sample_format))
	{
		throw std::runtime_error{"Unknown sample format"};
	}

	ret.sample_format = [](auto bits_per_sample, auto sample_format) {
		if(bits_per_sample == 32 && sample_format == SAMPLEFORMAT_IEEEFP)
		{
			return sample_format::float_32;
		}
		else
		{
			throw std::runtime_error{"Unsupported sample format"};
		}
	}(bits_per_sample, SAMPLEFORMAT_IEEEFP);

	uint16_t channel_count{};
	if(!TIFFGetField(handle, TIFFTAG_SAMPLESPERPIXEL, &channel_count))
	{
		throw std::runtime_error{"Unknown channel count"};
	}
	ret.channel_count = channel_count;

	ret.layout = get_image_layout(handle);

	return ret;
}

std::unique_ptr<GTIF, gtif_releaser> make_gtif(TIFF* handle)
{
	std::unique_ptr<GTIF, gtif_releaser> ret{GTIFNew(handle)};
	if(ret.get() == nullptr)
	{
		throw std::runtime_error{"Failed fetch GeoTIFF data"};
	}

	int versions[3];
	GTIFDirectoryInfo(ret.get(), versions, 0);
	if (versions[1] > 1)
	{
		throw std::runtime_error{"Unsupported GeoTIFF version"};
	}

	geocode_t model;    /* all key-codes are of this type */
	if (!GTIFKeyGet(ret.get(), GTModelTypeGeoKey, &model, 0, 1))
	{
		throw std::runtime_error{"No model type present"};
	}

	return ret;
}

corners_in_geo_coords get_domain(GTIF* handle, const GTIFDefn& defn, image_size img_size)
{
	if(defn.Model != ModelTypeGeographic)
	{
		throw std::runtime_error{"Usupported format"};
	}

    auto raster_mode = RasterPixelIsArea;
    GTIFKeyGet(handle, GTRasterTypeGeoKey, &raster_mode, 0, 1);
	auto const box_offset = (raster_mode == RasterPixelIsArea) ? 0.0f : -0.5f;
    vec4_t const img_coords_min{box_offset, box_offset, 0.0f, 0.0f};
	vec4_t const img_coords_max = img_coords_min + vec4_t{static_cast<float>(img_size.sizes[0]),
		static_cast<float>(img_size.sizes[1]), 0.0f, 0.0f};

	auto const get_geo_coords = [handle](vec4_t loc) {
		double x{loc[0]};
		double y{loc[1]};

		if( !GTIFImageToPCS(handle, &x, &y ) )
		{ throw std::runtime_error{"GTIFImageToPCS failed"}; }

		return std::numbers::pi_v<float>*vec4_t{static_cast<float>(x), static_cast<float>(y), 0.0f, 0.0f}/180.0f;
	};

	return corners_in_geo_coords{get_geo_coords(img_coords_min), get_geo_coords(img_coords_max)};
}