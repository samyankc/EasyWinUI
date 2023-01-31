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

// template<std::integral T>
// constexpr auto IntegralValueSamples = MakeHomogeneousTuple<T>( 0, 1, 2, -1, -2,             //
//                                                                MaxOf<T> / 2, MinOf<T> / 2,  //
//                                                                MaxOf<T>, MinOf<T> );

// template<std::integral T>
// constexpr auto IntegralValueSamples =
//     std::initializer_list<T>{ 0, 1, 2, -1, -2, MaxOf<T> / 2, MinOf<T> / 2, MaxOf<T>, MinOf<T> };

template<std::integral T>
constexpr auto IntegralValueSamples =
    IntegralConstantTuple<T, 0, 1, 2, -1, -2, MaxOf<T> / 2, MinOf<T> / 2, MaxOf<T>, MinOf<T>>;

constexpr auto IntegralTypeSamples = std::tuple<char, short, int, long long,    //
                                                unsigned char, unsigned short,  //
                                                unsigned int, unsigned long long>{};

int main()
{
    // test( "AlwaysReturn<N>() is same as N" ) = [] {
    //     expect( AlwaysReturn<0>() == 0 );
    //     expect( AlwaysReturn<1>() == 1 );
    //     expect( AlwaysReturn<3>() == 3 );
    //     expect( AlwaysReturn<4>() == _t(3) );
    // };

    // "AlwaysReturn<N>() is same as N"_test = [] {
    //     ConstexprForEachType<char, short, int, long long,                         //
    //                          unsigned char, unsigned short,                       //
    //                          unsigned int, unsigned long long>( []<typename T> {  //
    //         ConstexprUnroll<5>( []<T N> {                                         //
    //             constexpr T N_T = static_cast<T>( N );
    //             expect( AlwaysReturn<N_T>() == _t( N_T ) );
    //         } );
    //     } );
    // };

    "AlwaysReturn<N>() should be the same as N"_test = []<typename T> {
        test( "Type = " + TypeName<T> ) = []<typename IntConst> {
            constexpr auto N = IntConst::value;
            expect( AlwaysReturn<N>() == _t( N ) ) << "Value = " + std::to_string( N );
            expect( static_cast<T>( AlwaysReturn<N> ) == N / 2 ) << "Value = " + std::to_string( N );
        } | IntegralValueSamples<T>;
    } | IntegralTypeSamples;

    // "Preserving Type Completeness"_test = [] {
    //     ConstexprForEachType<int, long long, unsigned char>( []<typename T> {

    //     } );
    // };

    // "Function Pointer Decay"_test = []( auto F ) {
    //     constexpr auto N = 123;
    //     auto A = static_cast<decltype( F )>( AlwaysReturn<N> )(
    //         {} );  //this is incorrect, need to accquire argument type instead
    //     auto R = F( AlwaysReturn<N> );

    //     expect( type<decltype( R )> == type<decltype( A )> ) << "return type mismatch";
    //     expect( R == A && R == N );
    // } | std::tuple{ &ConsumeFuncPtr1, &ConsumeFuncPtr2 };
}