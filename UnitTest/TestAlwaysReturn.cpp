#include <EasyMeta.h>
#include <EasyTest.h>


using namespace EasyMeta;
using namespace boost::ut;

using FuncPtr1 = int ( * )( int, char, int* );
using FuncPtr2 = double ( * )( int, long long, double* );

template<typename GetSizeFunction, auto... Args>
auto Consume( GetSizeFunction F )
{
    return F( Args... );
}
auto Consume_FuncPtr1( FuncPtr1 F ) { return F( {}, {}, {} ); }
auto Consume_FuncPtr2( FuncPtr2 F ) { return F( {}, {}, {} ); }

template<std::integral T>
constexpr auto IntegralValueSamples =
    std::integer_sequence<T, 0, 1, 2, -1, -2, MaxOf<T> / 2, MaxOf<T>, MinOf<T> / 2, MinOf<T>>{};

int main()
{
    "Invoke to be identity"_test = []<class TestType> {
        constexpr auto _ = []<TestType... Samples>( std::integer_sequence<TestType, Samples...> )
        {
            ( expect( AlwaysReturn<Samples>() == _t( Samples ) ), ... );
        }
        ( IntegralValueSamples<TestType> );
    } | std::tuple<int, long long, unsigned char>{};

    "Template Consume"_test = [] { expect( Consume( AlwaysReturn<123> ) == 123 ); };

    "Function Pointer Decay"_test = [] {
        expect( Consume_FuncPtr1( AlwaysReturn<123> ) == 123 );
        expect( Consume_FuncPtr2( AlwaysReturn<123> ) == 123 );
    };
}