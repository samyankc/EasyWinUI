#ifndef EASYMETA_H
#define EASYMETA_H

#include <algorithm>
#include <type_traits>
#include <string_view>

namespace EasyMeta
{

    template<typename Derived, typename Base>
    concept DerivedFrom = std::is_base_of_v<std::decay_t<Base>, std::decay_t<Derived>>;

    template<typename TestClass, template<typename...> typename TemplateClass>
    struct SpecializationOf_impl : std::false_type
    {};

    template<template<typename...> typename TemplateClass, typename... Ts>
    struct SpecializationOf_impl<TemplateClass<Ts...>, TemplateClass> : std::true_type
    {};

    template<typename TestClass, template<typename...> typename TemplateClass>
    concept SpecializationOf = SpecializationOf_impl<TestClass, TemplateClass>::value;

    template<typename TargetType, typename... CandidateTypes>
    concept MatchExactType = ( std::same_as<TargetType, CandidateTypes> || ... );

    template<typename TargetType, typename... CandidateTypes>
    concept MatchType = MatchExactType<std::decay_t<TargetType>, std::decay_t<CandidateTypes>...>;

    template<std::size_t N>
    struct FixedString
    {
        char data[N]{};
        constexpr FixedString( const char ( &Src )[N] ) { std::copy_n( Src, N, data ); }
        constexpr operator std::string_view() const noexcept { return { data, N }; }
        constexpr auto size() const noexcept { return N; }
    };

    // template<std::size_t N> FixedString( const char ( & )[N] ) -> FixedString<N>; // deduction guide not needed ?

}  // namespace EasyMeta
#endif