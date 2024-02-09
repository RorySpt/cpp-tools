#pragma once
#include <string>
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
//#include <Windows/MinWindows.h>
#include <Windows.h> // just for windows platform

extern std::string WideCharToAnsi(std::wstring_view ws);
extern std::wstring AnsiToWideChar(std::string_view s);
extern std::string WideCharToUtf8(std::wstring_view ws);
extern std::wstring Utf8ToWideChar(std::string_view u8s);
extern std::string AnsiToUtf8(std::string_view s);
extern std::string Utf8ToAnsi(std::string_view u8s);

extern void code_trans_unit_testing();

// Wait for cpp23... then #include<print> replaced

//template <class... Types>
//void println(const std::string_view fmt, Types&&... args) { std::cout << std::vformat(fmt, std::make_format_args(args...)) << "\n"; }
//template <class... Types>
//void print(const std::string_view fmt, Types&&... args) { std::cout << std::vformat(fmt, std::make_format_args(args...)); }


inline std::string WideCharToAnsi(std::wstring_view ws)
{
	if (ws.empty())return {};
	const int length = WideCharToMultiByte(CP_ACP, 0, ws.data(), static_cast<int>(ws.size()), nullptr, 0, nullptr, nullptr);
	if (length == 0) {
		printf("Conversion failed. Error code: %lu\n", GetLastError());
		return {};
	}
	std::string s; s.resize(length);
	if (!WideCharToMultiByte(CP_ACP, 0, ws.data(), static_cast<int>(ws.size()), s.data(), length, nullptr, nullptr)) {
		printf("Conversion failed. Error code: %lu\n", GetLastError());
		return {};
	}
	return s;
}

inline std::wstring AnsiToWideChar(std::string_view s)
{
	if (s.empty())return {};
	const int length = MultiByteToWideChar(CP_ACP, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
	if (length == 0) {
		printf("Conversion failed. Error code: %lu\n", GetLastError());
		return {};
	}
	std::wstring ws; ws.resize(length);
	if (!MultiByteToWideChar(CP_ACP, 0, s.data(), static_cast<int>(s.size()), ws.data(), length)) {
		printf("Conversion failed. Error code: %lu\n", GetLastError());
		return {};
	}
	return ws;
}

inline std::string WideCharToUtf8(std::wstring_view ws)
{
	if (ws.empty())return {};
	const int length = WideCharToMultiByte(CP_UTF8, 0, ws.data(), static_cast<int>(ws.size()), nullptr, 0, nullptr, nullptr);
	if (length == 0) {
		printf("Conversion failed. Error code: %lu\n", GetLastError());
		return {};
	}
	std::string u8s; u8s.resize(length);
	if (!WideCharToMultiByte(CP_UTF8, 0, ws.data(), static_cast<int>(ws.size()), reinterpret_cast<char*>(u8s.data()), length, nullptr, nullptr)) {
		printf("Conversion failed. Error code: %lu\n", GetLastError());
		return {};
	}
	return u8s;
}

inline std::wstring Utf8ToWideChar(std::string_view u8s)
{
	if (u8s.empty())return {};
	const int length = MultiByteToWideChar(CP_UTF8, 0, reinterpret_cast<const char*>(u8s.data()), static_cast<int>(u8s.size()), nullptr, 0);
	if (length == 0) {
		printf("Conversion failed. Error code: %lu\n", GetLastError());
		return {};
	}
	std::wstring ws; ws.resize(length);
	if (!MultiByteToWideChar(CP_UTF8, 0, reinterpret_cast<const char*>(u8s.data()), static_cast<int>(u8s.size()), ws.data(), length)) {
		printf("Conversion failed. Error code: %lu\n", GetLastError());
		return {};
	}
	return ws;
}

inline std::string AnsiToUtf8(std::string_view s)
{
	return WideCharToUtf8(AnsiToWideChar(s));
}

inline std::string Utf8ToAnsi(std::string_view u8s)
{
	return WideCharToAnsi(Utf8ToWideChar(u8s));
}

inline void code_trans_unit_testing()
{
	//println("common unit test begin!");
	//const std::wstring input_w = L"³É¹¦£¡";
	//const std::u8string input_u8 = WideCharToUtf8(input_w.c_str());
	//const std::string input = WideCharToAnsi(input_w.c_str());
	//
	//println("Ansi<->WideChar: {}", WideCharToAnsi(AnsiToWideChar(input.c_str()).c_str()));
	//println("Utf8<->WideChar: {}", WideCharToAnsi(Utf8ToWideChar(input_u8.c_str()).c_str()));
	//println("Utf8<->Ansi: {}", Utf8ToAnsi(AnsiToUtf8(input.c_str()).c_str()));
}

