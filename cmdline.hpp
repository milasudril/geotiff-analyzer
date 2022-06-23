#ifndef CMDLINE_HPP
#define CMDLINE_HPP

#include <map>
#include <string>
#include <stdexcept>
#include <algorithm>

class command_line
{
public:
	using storage_type = std::map<std::string, std::string, std::less<>>;
	using key_type = storage_type::key_type;
	using value_type = storage_type::value_type;
	using mapped_type = storage_type::mapped_type;

	explicit command_line(int argc, char** argv)
	{
		for(int k = 1; k < argc; ++k)
		{
			auto const arg = std::string_view{argv[k]};
			auto const i = std::ranges::find(arg, '=');
			if(i == std::end(arg))
			{ throw std::runtime_error{std::string{arg}.append(" is missing a value")}; }
			m_storage[std::string{std::begin(arg), i}] = std::string{i + 1, std::end(arg)};
		}
	}

	auto const& operator[](std::string_view key) const
	{
		auto i = m_storage.find(key);
		if(i == std::end(m_storage))
		{ throw std::runtime_error{std::string{"Mandatory arguemnt "}.append(key).append(" not given")}; }
		return i->second;
	}

	auto begin() const { return std::begin(m_storage); }

	auto end() const { return std::end(m_storage); }

	auto size() const { return std::size(m_storage); }

	auto find(std::string_view key) const
	{
		return m_storage.find(key);
	}

private:
	storage_type m_storage;
};


template<class T, class ... Args>
T get_or(command_line const& cmdline, std::string_view key, T default_val, Args&& ... args)
{
	auto i = cmdline.find(key);
	if(i == std::end(cmdline))
	{
		return default_val;
	}
	return T{i->second, std::forward<Args>(args)...};
}

template<class T, class ... Args>
T get_or(command_line const& cmdline, char const* key, T default_val, Args&& ... args)
{
	auto i = cmdline.find(key);
	if(i == std::end(cmdline))
	{
		return default_val;
	}
	return T{i->second, std::forward<Args>(args)...};
}

template<class T>
bool is_valid(std::optional<T> const& opt)
{
	return opt.has_value();
}

template<class T, class OptionalLike, class ... Args>
T get_or(OptionalLike const& val, T default_val, Args&& ... args)
{
	if(is_valid(val))
	{
		return T{*val, std::forward<Args>(args)...};
	}
	return default_val;
}

#endif