#ifndef H_QUERY_STRING_PARSER_
#define H_QUERY_STRING_PARSER_

#include "string_split.hpp"
#include <map>

auto MapQueryString( std::string_view Source )
{
    auto Result = std::map<std::string_view,std::string_view>{};

    for( auto Segment : Split( Source ).By( '&' ) )
    {
        auto [Key, Value] = Split( Segment ).By( '=' ) | Take<2>;
        Result[Key] = Value;
    }
    return Result;
}

#endif