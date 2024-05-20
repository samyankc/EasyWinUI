#include "boost-ext/ut.hpp"

// [[maybe_unused]] auto UnitTest() __attribute__ ((weak));
[[maybe_unused]] auto UnitTest() -> void;

#ifdef UNIT_TEST
int main() { UnitTest(); }
#define main( ... ) inline SkipOriginalMain( __VA_ARGS__ )
// #define ParenOpen (
// #define ParenClose )
// #define UnitTest( ... ) main ParenOpen __VA_ARGS__ ParenClose
#else
// disable reporting if UNIT_TEST not set while UnitTest() is defined
#define UnitTest( ... )                                                                   \
    _{ 0 };                                                                               \
    namespace ut = boost::ut;                                                             \
    namespace cfg                                                                         \
    {                                                                                     \
        class reporter                                                                    \
        {                                                                                 \
          public:                                                                         \
            auto on( ut::events::test_begin ) -> void {}                                  \
            auto on( ut::events::test_run ) -> void {}                                    \
            auto on( ut::events::test_skip ) -> void {}                                   \
            auto on( ut::events::test_end ) -> void {}                                    \
            template<class TMsg> auto on( ut::events::log<TMsg> ) -> void {}              \
            template<class TExpr> auto on( ut::events::assertion_pass<TExpr> ) -> void {} \
            template<class TExpr> auto on( ut::events::assertion_fail<TExpr> ) -> void {} \
            auto on( ut::events::fatal_assertion ) -> void {}                             \
            auto on( ut::events::exception ) -> void {}                                   \
            auto on( ut::events::summary ) -> void {}                                     \
        };                                                                                \
    }                                                                                     \
    template<> inline auto ut::cfg<ut::override> = ut::runner<cfg::reporter>{};           \
    auto UnitTest( __VA_ARGS__ )
#endif

#include <tuple>
#include <string>

using namespace boost::ut;
using namespace boost::ut::literals;

template<typename T>
constexpr auto TypeName = []<typename U> {
    auto TemplateTypeName = std::string_view{ __PRETTY_FUNCTION__ };
    TemplateTypeName.remove_suffix( 1 );
    TemplateTypeName.remove_prefix( TemplateTypeName.find( '=' ) + 2 );
    return TemplateTypeName;
}.template operator()<T>();

template<typename CharT = char> auto operator+( const CharT* LHS, std::basic_string_view<CharT> RHS )
{  //
    return LHS + std::basic_string<CharT>{ RHS };
}

// for easier building of test samples of the same type
// using std::array or std::initializer_list instead?
template<typename T> constexpr auto MakeHomogeneousTuple( const auto&... Args )
{  //
    return std::make_tuple( static_cast<T>( Args )... );
}

template<typename T, auto... Args> constexpr auto IntegralConstantTuple = std::tuple<std::integral_constant<T, static_cast<T>( Args )>...>{};
