#ifndef FILE_HPP
#define FILE_HPP

#include <cstdio>
#include <memory>
#include <stdexcept>

struct file_releaser
{
	void operator()(FILE* handle) const
	{
		if(handle != nullptr)
		{ fclose(handle); }
	}
};

class file
{
public:
	using handle = std::unique_ptr<FILE, file_releaser>;

	file() = default;

	template<class ... Args>
	explicit file(std::string const& filename, Args&& ... args):
		m_handle{fopen(filename.c_str(), std::forward<Args>(args)...)}
	{
		if(m_handle == nullptr)
		{
			throw std::runtime_error{std::string{"Failed to open "}.append(filename)};
		}
	}

	auto get() const
	{ return m_handle.get(); }

	FILE* operator*() const { return get(); }

private:
	handle m_handle;
};

inline bool is_valid(file const& f)
{
	return f.get() != nullptr;
}

#endif