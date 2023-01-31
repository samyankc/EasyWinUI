#include <boost-ext/ut.hpp>
#include <EasyMeta.h>
#include <tuple>
#include <string>
#include <typeinfo>
#include <cxxabi.h>  //abi::__cxa_demangle()

template<typename T>
constexpr auto TypeName_imp() noexcept
{
    auto TemplateTypeName = std::string_view{ __PRETTY_FUNCTION__ };
    TemplateTypeName.remove_suffix( 1 );
    TemplateTypeName.remove_prefix( TemplateTypeName.find( '=' ) + 2 );
    return TemplateTypeName;
}

template<typename T>
constexpr auto TypeName = TypeName_imp<T>();

template<typename CharT = char>
auto operator+( const CharT* LHS, std::basic_string_view<CharT> RHS )
{
    return LHS + std::basic_string<CharT>{ RHS };
}

// for easier building of test samples of the same type
// using std::array or std::initializer_list instead?
template<typename T>
constexpr auto MakeHomogeneousTuple( const auto&... Args )
{
    return std::make_tuple( static_cast<T>( Args )... );
}

template<typename T, auto... Args>
constexpr auto IntegralConstantTuple = std::tuple<std::integral_constant<T, static_cast<T>( Args )>...>{};

// for supporting pipe semantic
