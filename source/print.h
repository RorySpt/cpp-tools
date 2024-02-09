#pragma once
#include <format>
#include <string_view>


constexpr inline struct tag_out_path_t {} tag_out_path;

template<typename T>
concept ostream_r_rr = std::is_same_v<std::remove_reference_t<T>, std::ofstream>;

inline void sync_out(ostream_r_rr auto&& stream, std::string_view s, bool new_line = false)
{
	if (new_line)
	{
		const std::string ss{ std::string(s) + '\n' };
		std::osyncstream(stream).write(ss.data(), ss.size());
	}
	else
	{
		std::osyncstream(stream).write(s.data(), s.size());
	}

}

inline void sync_out(FILE* file, std::string_view s, bool new_line = false)
{
	if (new_line)
	{
		const std::string ss{ std::string(s) + '\n' };
		fwrite(ss.data(), sizeof(char), ss.size(), file);  // NOLINT(cert-err33-c)
	}
	else
	{
		fwrite(s.data(), sizeof(char), s.size(), file);  // NOLINT(cert-err33-c)
	}
}
inline void sync_out(std::string_view s, bool new_line = false)
{
	sync_out(stdout, s, new_line);
}

template <class... Types> requires (sizeof...(Types) > 0)
_NODISCARD void print(const std::_Fmt_string<Types...> Fmt, Types&&... Args) {
	sync_out(_STD vformat(Fmt._Str, _STD make_format_args(Args...)));
}
template <int = 0>
void print(std::string_view s)
{
	sync_out(s);
}
template <class... Types> requires (sizeof...(Types) > 0)
_NODISCARD void println(const std::_Fmt_string<Types...> Fmt, Types&&... Args) {
	sync_out(_STD vformat(Fmt._Str, _STD make_format_args(Args...)), true);
}
template <int = 0>
void println(std::string_view s = "")
{
	sync_out(s, true);
}
template <int = 0>
void println(const char* s)
{
	if(s != nullptr) sync_out(s, true);
}
template <class... Types> requires (sizeof...(Types) > 0)
_NODISCARD void print(FILE* file, const std::_Fmt_string<Types...> Fmt, Types&&... Args) {
	sync_out(file, _STD vformat(Fmt._Str, _STD make_format_args(Args...)));
}
template <int = 0>
void print(FILE* file, std::string_view s)
{
	sync_out(file, s);
}
template <class... Types> requires (sizeof...(Types) > 0)
_NODISCARD void println(FILE* file, const std::_Fmt_string<Types...> Fmt, Types&&... Args) {
	sync_out(file, _STD vformat(Fmt._Str, _STD make_format_args(Args...)), true);
}
template <int = 0>
void println(FILE* file, std::string_view s)
{
	sync_out(file, s, true);
}

template <class... Types> requires (sizeof...(Types) > 0)
_NODISCARD void print(ostream_r_rr auto&& file, const std::_Fmt_string<Types...> Fmt, Types&&... Args) {
	sync_out(file, _STD vformat(Fmt._Str, _STD make_format_args(Args...)));
}
template <ostream_r_rr stream_t>
void print(stream_t&& file, std::string_view s)
{
	sync_out(std::forward<stream_t>(file), s);
}
template <ostream_r_rr stream_t, class... Types> requires (sizeof...(Types) > 0)
_NODISCARD void println(stream_t&& file, const std::_Fmt_string<Types...> Fmt, Types&&... Args) {
	sync_out(std::forward<stream_t>(file), _STD vformat(Fmt._Str, _STD make_format_args(Args...)), true);
}
template <ostream_r_rr stream_t>
void println(stream_t&& file, std::string_view s)
{
	sync_out(std::forward<stream_t>(file), s, true);
}

template <class... Types> requires (sizeof...(Types) > 0)
_NODISCARD void print(tag_out_path_t, std::string_view path, const std::_Fmt_string<Types...> Fmt, Types&&... Args) {
	sync_out(std::ofstream(path.data()), _STD vformat(Fmt._Str, _STD make_format_args(Args...)));
}
template <int = 0>
void print(tag_out_path_t, std::string_view path, std::string_view s)
{
	sync_out(std::ofstream(path.data()), s);
}
template <class... Types> requires (sizeof...(Types) > 0)
_NODISCARD void println(tag_out_path_t, std::string_view path, const std::_Fmt_string<Types...> Fmt, Types&&... Args) {
	sync_out(std::ofstream(path.data()), _STD vformat(Fmt._Str, _STD make_format_args(Args...)), true);
}
template <int = 0>
void println(tag_out_path_t, std::string_view path, std::string_view s)
{
	sync_out(std::ofstream(path.data()), s, true);
}
