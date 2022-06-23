//@	{"target":{"name":"geotiff2exr.o", "pkgconfig_libs":["OpenEXR"]}}

#include "./geotiff_loader.hpp"
#include "./file.hpp"

#include <OpenEXR/ImfIO.h>
#include <OpenEXR/ImfOutputFile.h>
#include <OpenEXR/ImfChannelList.h>

int main(int argc, char** argv)
{
	if(argc != 3)
	{
		fprintf(stderr, "Usage: geotiff2exr input output\n");
		return 1;
	}

	auto tiff = make_tiff(argv[1]);
	auto const info = get_image_info(tiff.get());
	auto pixels = load_floats(tiff.get(), info);
	auto src_ptr = static_cast<float const*>(pixels.get());
	auto const w = info.size.sizes[0];
	auto const h = info.size.sizes[1];

	Imf::Header header{static_cast<int>(w), static_cast<int>(h)};
	header.channels().insert("Y", Imf::Channel{Imf::FLOAT});

	Imf::FrameBuffer fb;
	fb.insert("Y",
	          Imf::Slice{Imf::FLOAT,
	                     (char*)(src_ptr),
	                     sizeof(float),
	                     sizeof(float) * w});

	Imf::OutputFile dest{argv[2], header};
	dest.setFrameBuffer(fb);
	dest.writePixels(h);

	return 0;
}

