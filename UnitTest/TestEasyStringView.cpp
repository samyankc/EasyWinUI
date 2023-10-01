#include "../EasyStringView.h"
#include <iostream>
#include <format>

int main()
{
    {
        auto s = EasyStringView{ "[Const Char *]" };
        std::cout << std::format( "Content={}, Null Terminated = {} \n",  //
                                  static_cast<std::string_view>( s ), s.NullTerminated );
    }

    {
        auto sv = std::string_view{ "[Const Char *]" };
        auto s = EasyStringView{ sv };
        std::cout << std::format( "Content={}, Null Terminated = {} \n",  //
                                  static_cast<std::string_view>( s ), s.NullTerminated );
    }

}