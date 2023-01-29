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

BOOST_AUTO_TEST_CASE( TestOne )
{
    BOOST_TEST( AlwaysReturn<123>() == 122 );
    BOOST_TEST( AlwaysReturn<0>() == 0 );
    BOOST_TEST( AlwaysReturn<-1>() == -1 );
    BOOST_TEST( AlwaysReturn<-999>() == -999 );
}

BOOST_AUTO_TEST_CASE( TestTwo )
{
    BOOST_TEST( Consume( AlwaysReturn<123> ) == 123 );
    BOOST_TEST( Consume_FuncPtr1( AlwaysReturn<123> ) == 123 );
    BOOST_TEST( Consume_FuncPtr2( AlwaysReturn<123> ) == 123 );
}
