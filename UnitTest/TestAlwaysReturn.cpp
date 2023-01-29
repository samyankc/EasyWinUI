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
auto Consume( FuncPtr1 F ) { return F( {}, {}, {} ); }
auto Consume( FuncPtr2 F ) { return F( {}, {}, {} ); }

template<std::integral T>
constexpr auto IntegralValueSamples = std::integer_sequence<T, 0, 1, 2, static_cast<T>( -1 ), static_cast<T>( -2 ),
                                                            MaxOf<T> / 2, MinOf<T> / 2, MaxOf<T>, MinOf<T>>{};

int main()
{
    "Invoke to be identity"_test = []<typename TestType> {
        []<TestType... Samples>( std::integer_sequence<TestType, Samples...> )
        {
            ( ( test( "N = " + std::to_string( Samples ) ) =
                    [] {  //
                        expect( AlwaysReturn<Samples>() == _t( Samples ) );
                    } ),
              ... );
        }
        ( IntegralValueSamples<TestType> );
    } | std::tuple<int, long long, unsigned char>{};

    "Template Consume"_test = [] { expect( Consume( AlwaysReturn<123> ) == 123 ); };

    "Function Pointer Decay"_test = []<typename FunctionPointer> {
        auto A = static_cast<FunctionPointer>( AlwaysReturn<123> )( {}, {}, {} );
        auto R = Consume( static_cast<FunctionPointer>( AlwaysReturn<123> ) );

        expect( std::is_same_v<decltype( R ), decltype( A )> ) << "return type mismatch";
        expect( R == A );
    } | std::tuple<FuncPtr1, FuncPtr2>{};
}