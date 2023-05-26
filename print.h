//#define FMT_HEADER_ONLY

#ifndef NON_STANDARD_PRINT_H_
#define NON_STANDARD_PRINT_H_

#include <format>
#include <iostream>

namespace std
{
    template<typename... T>
    constexpr void print( std::format_string<T...> fmt, T&&... Args )
    {
        std2::print(fmt, std::forward<T>( Args )...);
        //std::format_to( std::ostreambuf_iterator( std::cout ), fmt, std::forward<T>( Args )... );
    }

    template<typename... T>
    constexpr void println( std::format_string<T...> fmt, T&&... Args )
    {
        std2::print( fmt, Args... );
        std2::print( "\n" );
    }

}  // namespace std

#endif