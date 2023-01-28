#ifndef EASYMETA_H
#define EASYMETA_H

#include <algorithm>
#include <type_traits>
#include <string_view>
#include <array>

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

    template<typename T, T V>
    struct integral_constant_extension : std::integral_constant<T, V>
    {
        template<typename R, typename... Args>
        constexpr static R PointerDecay( Args... ) noexcept
        {
            return V;
        }

        template<typename R = T, typename... Args,  //
                 typename FunctionPointer = R ( * )( Args... )>
        constexpr operator FunctionPointer() const noexcept
        {
            return static_cast<FunctionPointer>( PointerDecay );
        }

        constexpr operator T() const noexcept { return V; }
        constexpr auto operator()( const auto&... ) const noexcept { return V; }
    };

    template<auto V>
    constexpr auto AlwaysReturn = integral_constant_extension<decltype( V ), V>{};

    template<std::size_t N, typename CharT>
    struct FixedString
    {
        CharT data[N];
        constexpr FixedString( const CharT ( &Src )[N] ) { std::copy_n( Src, N, data ); }
        constexpr operator std::basic_string_view<CharT>() const noexcept { return { data }; }
        //constexpr auto sv() const noexcept { return std::basic_string_view{ data }; }
        constexpr auto operator[]( std::size_t i ) const noexcept { return data[i]; }
        constexpr auto BufferSize() const noexcept { return N; }
    };

    //template<std::size_t N, typename CharT> FixedString( const CharT ( & )[N] ) -> FixedString<N, CharT>;

    template<typename NewCharT, FixedString Src>
    consteval auto MakeFixedString()
    {
        constexpr auto PrecisionPreservation =
            std::ranges::all_of( Src.data, []( auto c ) { return c <= std::numeric_limits<NewCharT>::max(); } );

        static_assert( PrecisionPreservation, "Loss of Precision From Conversion" );

        NewCharT data[Src.BufferSize()]{};
        std::ranges::copy( Src.data, data );
        return FixedString{ data };
    }

}  // namespace EasyMeta
#endif