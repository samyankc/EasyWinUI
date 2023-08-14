#ifndef H_FCGI_REQUEST_PARSER_
#define H_FCGI_REQUEST_PARSER_

#include "string_split.hpp"
#include <fcgi_stdio.h>
#include <string>
#include <string_view>
#include <algorithm>
#include <vector>
#include <array>
#include <optional>
#include <map>

namespace FCGI
{
    auto MapQueryString( std::string_view Source )
    {
        auto Result = std::map<std::string_view, std::string_view>{};

        for( auto Segment : Split( Source ).By( '&' ) )
        {
            auto [Key, Value] = Split( Segment ).By( '=' ) | Take<2>;
            Result[Key] = Value;
        }
        return Result;
    }

    namespace HTTP
    {
        struct ReuqestMethod
        {
            enum class VerbType { INVALID = 0, GET, HEAD, POST, PUT, DELETE, CONNECT, OPTIONS, TRACE, PATCH } Verb;
            using enum VerbType;
            inline const static auto StrViewToVerb =
                std::map<std::string_view, VerbType>{ { "GET", GET },   { "DELETE", DELETE },   { "TRACE", TRACE },
                                                      { "POST", POST }, { "CONNECT", CONNECT }, { "HEAD", HEAD },
                                                      { "PUT", PUT },   { "OPTIONS", OPTIONS }, { "PATCH", PATCH },
                                                      { "", INVALID } };

            ReuqestMethod() = default;
            ReuqestMethod( const ReuqestMethod& ) = default;
            ReuqestMethod( VerbType OtherVerb ) : Verb{ OtherVerb } {}
            ReuqestMethod( std::same_as<std::string_view> auto VerbName )
                : Verb{ StrViewToVerb.contains( VerbName ) ? StrViewToVerb.at( VerbName ) : INVALID }
            {}

            constexpr operator std::string_view() const
            {
                using enum VerbType;
                switch( Verb )
                {
                    default : return "INVALID";
                    case GET : return "GET";
                    case HEAD : return "HEAD";
                    case POST : return "POST";
                    case PUT : return "PUT";
                    case DELETE : return "DELETE";
                    case CONNECT : return "CONNECT";
                    case OPTIONS : return "OPTIONS";
                    case TRACE : return "TRACE";
                    case PATCH : return "PATCH";
                }
            }

            constexpr operator VerbType() const { return Verb; }
        };

    }  // namespace HTTP

    struct Request
    {
        HTTP::ReuqestMethod RequestMethod;
        size_t ContentLength;
        std::string_view ContentType;
        std::string_view ScriptName;
        std::string_view RequestURI;
        std::string QueryStringCache;
        std::map<std::string_view, std::string_view> QueryString;

        auto Read( std::string_view ParamName )
        {
            return std::string_view{ getenv( std::string{ ParamName }.c_str() ) };
        }
    };

    auto NextRequest()
    {
        auto R = Request{};
        R.ContentLength = std::atoi( getenv( "CONTENT_LENGTH" ) );
        R.RequestMethod = getenv( "REQUEST_METHOD" );
        R.ContentType = getenv( "CONTENT_TYPE" );
        R.ScriptName = getenv( "SCRIPT_NAME" );
        R.RequestURI = getenv( "REQUEST_URI" );

        if( R.RequestMethod == HTTP::ReuqestMethod::POST )
            R.QueryStringCache.resize_and_overwrite(  //
                R.ContentLength,
                []( char* B, size_t S ) {  //
                    return FCGI_fread( B, 1, S, FCGI_stdin );
                }  //
            );
        else
            R.QueryStringCache = getenv( "QUERY_STRING" );

        R.QueryString = MapQueryString( R.QueryStringCache );

        return R;
    }
}  // namespace FCGI
#endif