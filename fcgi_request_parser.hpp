#ifndef H_FCGI_REQUEST_PARSER_
#define H_FCGI_REQUEST_PARSER_

// #include "string_split.hpp"
#include "BijectiveMap.hpp"
#include "EasyString.h"
#include <fcgi_stdio.h>

namespace FCGI
{
    using namespace EasyString;

    inline auto MapQueryString( std::string_view Source )
    {
        auto Result = BijectiveMap<std::string_view, std::string_view>{};

        for( auto Segment : Split::Lazy( Source ).By( '&' ) )
        {
            auto [Key, Value] = Split::Eager( Segment ).By( '=' ) | Bundle<2>;
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
            inline const static auto StrViewToVerb = BijectiveMap<std::string_view, VerbType>  //
                {                                                                              //
                  { "GET", GET },  { "POST", POST },    { "CONNECT", CONNECT }, { "TRACE", TRACE },
                  { "PUT", PUT },  { "HEAD", HEAD },    { "OPTIONS", OPTIONS }, { "PATCH", PATCH },
                  { "", INVALID }, { "DELETE", DELETE }
                };

            inline const static auto VerbToStrView = StrViewToVerb.Inverse();

            ReuqestMethod() = default;
            ReuqestMethod( const ReuqestMethod& ) = default;
            ReuqestMethod( VerbType OtherVerb ) : Verb{ OtherVerb } {}
            ReuqestMethod( std::convertible_to<std::string_view> auto VerbName ) : Verb{ StrViewToVerb[VerbName] } {}

            constexpr operator std::string_view() const { return VerbToStrView[Verb]; }

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
        BijectiveMap<std::string_view, std::string_view> QueryString;

        auto Read( std::string_view ParamName )
        {
            auto LoadParamFromEnv = getenv( std::string{ ParamName }.c_str() );
            return std::string_view{ LoadParamFromEnv ? LoadParamFromEnv : "No Content" };
        }
    };

    inline auto NextRequest()
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