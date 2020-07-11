#ifndef printstr_h
#define printstr_h

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
};

#endif
