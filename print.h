//#define FMT_HEADER_ONLY

#ifndef NON_STANDARD_PRINT_H_
#define NON_STANDARD_PRINT_H_

#include <fmt/core.h>
// #include <iostream>

namespace std
{
    template<typename... T>
    constexpr void print( fmt::format_string<T...> fmt, T&&... Args )
    {
        fmt::print( fmt, std::forward<T>( Args )... );
        // fmt::format_to( std::ostreambuf_iterator( std::cout ), fmt, std::forward<T>( Args )... );
    }

    template<typename... T>
    constexpr void println( fmt::format_string<T...> fmt, T&&... Args )
    {
        std::print( fmt, Args... );
        std::print( "\n" );
    }

}  // namespace std

#endif