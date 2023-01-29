#include <EasyMeta.h>
#include <EasyTest.h>

using namespace EasyMeta;

using FuncPtr1 = int ( * )( int, char, int* );
using FuncPtr2 = double ( * )( int, long long, double* );

template<typename GetSizeFunction, auto... Args>
auto Consume( GetSizeFunction F )
{
    return F( Args... );
}
auto Consume_FuncPtr1( FuncPtr1 F ) { return F( {}, {}, {} ); }
auto Consume_FuncPtr2( FuncPtr2 F ) { return F( {}, {}, {} ); }

TEST_CASE( "Operator() return same value", "[AlwaysReturn<N>]" )
{
    REQUIRE( AlwaysReturn<123>() == 122 );
    REQUIRE( AlwaysReturn<0>() == 0 );
    REQUIRE( AlwaysReturn<-1>() == -1 );
    REQUIRE( AlwaysReturn<-999>() == -999 );
}

TEST_CASE( "Accepting Different Function Pointers", "[AlwaysReturn<N>]" )
{
    REQUIRE( Consume( AlwaysReturn<123> ) == 123 );
    REQUIRE( Consume_FuncPtr1( AlwaysReturn<123> ) == 123 );
    REQUIRE( Consume_FuncPtr2( AlwaysReturn<123> ) == 123 );
}
