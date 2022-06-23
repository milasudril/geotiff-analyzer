//@	{"target":{"name":"skip_bytes.o"}}

#include "./cmdline.hpp"

#include <cstdio>
#include <cstdlib>

template<std::integral Int>
struct value
{
	value():val{0}{}

	explicit value(std::string const& str)
	{
		val = atoll(str.c_str());
	}

	Int get() const { return val;}

	Int val;
};

int main(int argc, char** argv)
{
	command_line const opts{argc, argv};

	auto const offset = get_or(opts, "offset", value<size_t>{}).get();
	auto const stride = get_or(opts, "stride", value<size_t>{}).get();

	size_t bytes_read = 0;
	size_t bytes_written = 0;
	while(true)
	{
		auto ch_in = getchar();

		if(ch_in == EOF)
		{ break; }

		if((bytes_read + offset)%stride == 0)
		{
			putchar(ch_in);
			++bytes_written;
		}
		++bytes_read;
	}

	fprintf(stderr, "%zu bytes in, %zu bytes out\n", bytes_read, bytes_written);
	return 0;
}