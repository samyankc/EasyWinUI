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
auto ConsumeFuncPtr1( FuncPtr1 F ) { return F( {}, {}, {} ); }
auto ConsumeFuncPtr2( FuncPtr2 F ) { return F( {}, {}, {} ); }

template<std::integral T>
constexpr auto IntegralValueSamples = std::integer_sequence<T, 0, 1, 2, static_cast<T>( -1 ), static_cast<T>( -2 ),
                                                            MaxOf<T> / 2, MinOf<T> / 2, MaxOf<T>, MinOf<T>>{};

int main()
{
    // test( "AlwaysReturn<N>() is same as N" ) = [] {
    //     expect( AlwaysReturn<0>() == 0 );
    //     expect( AlwaysReturn<1>() == 1 );
    //     expect( AlwaysReturn<3>() == 3 );
    //     expect( AlwaysReturn<4>() == _t(3) );
    // };

    "AlwaysReturn<N>() is same as N"_test = [] {
        ConstexprForEachType<char, short, int, long long,                         //
                             unsigned char, unsigned short,                       //
                             unsigned int, unsigned long long>( []<typename T> {  //
            ConstexprUnroll<100>( []<T N> {                                       //
                constexpr T N_T = static_cast<T>( N );
                expect( AlwaysReturn<N_T>() == _t( N_T ) );
            } );
        } );
    };

    // "Invoke to be identity"_test = []<typename TestType> {
    //     []<TestType... Samples>( std::integer_sequence<TestType, Samples...> )
    //     {
    //         ( ( test( "N = " + std::to_string( Samples ) ) =
    //                 [] {  //
    //                     expect( AlwaysReturn<Samples>() == _t( Samples ) );
    //                 } ),
    //           ... );
    //     }
    //     ( IntegralValueSamples<TestType> );
    // } | std::tuple<int, long long, unsigned char>{};

    // "Template Consume"_test = [] { expect( Consume( AlwaysReturn<123> ) == 123 ); };

    "Preserving Type Completeness"_test = [] {
        ConstexprForEachType<int, long long, unsigned char>( []<typename T> {

        } );
    };

    "Function Pointer Decay"_test = []( auto F ) {
        constexpr auto N = 123;
        auto A = static_cast<decltype( F )>( AlwaysReturn<N> )( {} );
        auto R = F( AlwaysReturn<N> );

        expect( type<decltype( R )> == type<decltype( A )> ) << "return type mismatch";
        expect( R == A && R == N );
    } | std::tuple{ &ConsumeFuncPtr1, &ConsumeFuncPtr2 };
}