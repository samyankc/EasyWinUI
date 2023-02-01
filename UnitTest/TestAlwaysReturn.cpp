#include <EasyMeta.h>
#include <EasyTest.h>

using namespace EasyMeta;
using namespace boost::ut;

template<std::integral T>
constexpr auto IntegralValueSamples =
    IntegralConstantTuple<T, 0, 1, 2, -1, -2, MaxOf<T> / 2, MinOf<T> / 2, MaxOf<T>, MinOf<T>>;

constexpr auto IntegralTypeSamples = std::tuple<char, short, int, long long,    //
                                                unsigned char, unsigned short,  //
                                                unsigned int, unsigned long long>{};

template<typename T>
constexpr auto FuncArgPair = 0;

template<typename R, typename... Args>
constexpr auto FuncArgPair<R( Args... )> = std::pair<R ( * )( Args... ), std::tuple<Args...>>{};

constexpr auto FunctionPointerSamples =
    std::make_tuple( FuncArgPair<int()>, FuncArgPair<int( int )>, FuncArgPair<int( char )>,
                     FuncArgPair<int( int, int )>, FuncArgPair<int( int, char* )>, FuncArgPair<double( int, int )> );

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
            expect( AlwaysReturn<N>() == _t( N ) );
        } | IntegralValueSamples<T>;
    } | IntegralTypeSamples;

    "AlwaysReturn<N> should be convertible to function pointer"_test = []<typename FuncArgSample> {
        using FuncPtr = typename FuncArgSample::first_type;
        using ArgTuple = typename FuncArgSample::second_type;
        test( "Converting to " + TypeName<FuncPtr> ) = []<typename IntConst> {
            constexpr auto N = IntConst::value;
            auto InvokeResult = std::apply( static_cast<FuncPtr>( AlwaysReturn<N> ), ArgTuple{} );
            "Invoke result should be same as N"_test = [InvokeResult, N] { expect( InvokeResult == _t( N ) ); };
            "Invoke result type should be same as / convertible from N"_test = [InvokeResult, N] {
                using InvokeResultType = std::remove_cv_t<decltype( InvokeResult )>;
                using NType = std::remove_cv_t<decltype( N )>;
                expect( type<InvokeResultType> == type<NType> ||
                        type_traits::is_convertible_v<NType, InvokeResultType> );
            };
        } | IntegralValueSamples<int>;
    } | FunctionPointerSamples;
}