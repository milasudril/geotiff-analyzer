#ifndef BLOB_HPP
#define BLOB_HPP

#include <cstdio>
#include <memory>
#include <stdexcept>
#include <string>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/unistd.h>

template<class T>
class blob
{
public:
	using handle = std::unique_ptr<T[]>;

	blob() = default;

	explicit blob(FILE* fptr, size_t N):m_handle{std::make_unique<T[]>(N)}
	{
		struct stat statbuf{};
		fstat(fileno(fptr), &statbuf);
		if(static_cast<size_t>(statbuf.st_size) != N)
		{
			throw std::runtime_error{std::string{"Failed to load blob: Wrong file size"}
				.append(" ")
				.append(std::to_string(statbuf.st_size))
				.append(" vs ")
				.append(std::to_string(N))};
		}

		auto const res = fread(get(), sizeof(T), N, fptr);
		if(res != N)
		{
			throw std::runtime_error{std::string{"Failed to load blob: Early EOF?"}
				.append(" ")
				.append(std::to_string(res))
				.append(" vs ")
				.append(std::to_string(N))};
		}
	}

	auto get() const
	{ return m_handle.get(); }

private:
	handle m_handle;
};

#endif