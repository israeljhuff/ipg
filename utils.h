#ifndef utils_h
#define utils_h

#include <cstdint>
#include <iostream>

namespace IPG
{
// ----------------------------------------------------------------------------
// variadic wrapper functions for printing to cout and cerr
// single-argument
template <typename T>
void printstr(std::ostream &strm, std::string sep, std::string term, T t)
{
  strm << t << term;
}

// multi-argument
template<typename T, typename... Args>
void printstr(std::ostream &strm, std::string sep, std::string term, T t, Args... args)
{
  strm << t << sep;
  printstr(strm, sep, term, args...);
}

// cout, no separator, no terminator
template<typename T, typename... Args>
void prints(T t, Args... args)
{
  printstr(std::cout, "", "", t, args...);
}

// cerr, no separator, no terminator
template<typename T, typename... Args>
void eprints(T t, Args... args)
{
  printstr(std::cerr, "", "", t, args...);
}

// cout, no separator, newline-terminated
template<typename T, typename... Args>
void println(T t, Args... args)
{
  printstr(std::cout, "", "\n", t, args...);
}

// cerr, no separator, newline-terminated
template<typename T, typename... Args>
void eprintln(T t, Args... args)
{
  printstr(std::cerr, "", "\n", t, args...);
}

// ----------------------------------------------------------------------------
// decode a utf-8 character into a 32-bit number
// on success, writes extracted code bits to num
// returns number of bytes from utf-8 on success or -1 on failure
int32_t utf8_to_int32(int32_t *num, const char *str)
{
	int32_t n_bytes;
	uint8_t byte = (uint8_t)str[0];
	uint32_t val;
	// in 1 byte case, return immediately
	if ((byte & 0x80) == 0) { *num = (int32_t)byte; return 1; }
	else if ((byte & 0xe0) == 0xc0) { n_bytes = 2; val = byte & 0x1f; }
	else if ((byte & 0xf0) == 0xe0) { n_bytes = 3; val = byte & 0x0f; }
	else if ((byte & 0xf8) == 0xf0) { n_bytes = 4; val = byte & 0x07; }
	else return -1;
	for (int32_t i = 1; i < n_bytes; i++)
	{
		val <<= 6;
		byte = (uint8_t)str[i];
		if ((byte & 0xc0) != 0x80) return -1;
		val += (int32_t)(byte & 0x3f);
	}
	*num = val;
	return n_bytes;
}
};

#endif
