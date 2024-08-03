#ifndef INDEXRANGE_H
#define INDEXRANGE_H

#include <concepts>
#include <utility>

#define FWD_( SELF ) std::forward<decltype( SELF )>( SELF )

namespace IndexRange
{
    template<typename DataType>  //
    struct IteratorData
    {
        DataType Current{};
    };

    struct IteratorMachinery
    {
        constexpr decltype( auto ) operator++( this auto&& Self ) { return FWD_( Self ) += 1; }
        constexpr decltype( auto ) operator--( this auto&& Self ) { return FWD_( Self ) += -1; }

        constexpr auto operator+( this auto Self, int N ) { return Self += N, Self; }
        constexpr auto operator-( this auto Self, int N ) { return Self += -N, Self; }

        template<typename SelfType, typename OtherType>  //
        // requires std::same_as<std::remove_cvref_t<SelfType>, std::remove_cvref_t<OtherType>>
        constexpr auto operator!=( this const SelfType& Self, const OtherType& Other )
        {
            return Self.Current != Other.Current;
        }
    };

    template<std::integral IntegralType>  //
    struct SelfReferencing : IteratorData<IntegralType>, IteratorMachinery
    {
        constexpr auto operator*( this auto Self ) { return Self.Current; }
        constexpr decltype( auto ) operator+=( this auto&& Self, IntegralType N ) { return Self.Current += N, FWD_( Self ); }
    };

    template<typename BaseIter>  //
    struct Iterator : IteratorData<BaseIter>, IteratorMachinery
    {
        constexpr decltype( auto ) operator*( this auto&& Self ) { return *Self.Current; }
        constexpr decltype( auto ) operator+=( this auto&& Self, int N ) { return Self.Current += N, FWD_( Self ); }
    };

    template<typename BaseIter>  //
    struct ReverseIterator : IteratorData<BaseIter>, IteratorMachinery
    {
        constexpr decltype( auto ) operator*( this auto&& Self ) { return *( Self.Current - 1 ); }
        constexpr decltype( auto ) operator+=( this auto&& Self, int N ) { return Self.Current += -N, FWD_( Self ); }
    };

    template<typename Iter>  //
    struct ForwardRange
    {
        constexpr ForwardRange( Iter Begin, Iter End ) : Begin{ Begin }, End{ End } {}
        constexpr auto begin() const { return Begin; }
        constexpr auto end() const { return End; }

      protected:
        Iter Begin;
        Iter End;
    };

    template<typename Iter, typename RIter = ReverseIterator<Iter>>  //
    struct ReverseRange : ForwardRange<RIter>
    {
        constexpr ReverseRange( Iter Begin, Iter End ) : ForwardRange<RIter>( { { End } }, { { Begin } } ) {}
    };

    template<std::integral IndexType = int, typename Iter = SelfReferencing<IndexType>>  //
    struct Range : ForwardRange<Iter>
    {
        constexpr Range( IndexType First, IndexType Last ) : ForwardRange<Iter>{ { First }, { Last + 1 } } {}
        constexpr Range( IndexType Distance ) : Range{ IndexType{}, { Distance - 1 } } {}
    };

    constexpr static struct Reverse
    {
    } Reverse;

    struct Drop
    {
        long long Count = 0;
    };
    struct Take
    {
        long long Count = 0;
    };

    template<typename Container, typename Adaptor>  //
    requires std::is_same_v<Adaptor, Drop> ||       //
             std::is_same_v<Adaptor, Take> ||       //
             std::is_same_v<Adaptor, struct Reverse>
    constexpr auto operator|( Container&& C, Adaptor A )
    {
        return C | A;
    }

    constexpr auto operator|( auto& Container, struct Reverse ) { return ReverseRange{ Container.begin(), Container.end() }; }

    constexpr auto PartitionLine( auto& Container, auto Amount )
    {
        auto NewBegin = Container.begin();
        auto End = Container.end();
        for( auto _ : Range( Amount.Count ) )
            if( NewBegin != End ) ++NewBegin;
        return NewBegin;
    }

    constexpr auto operator|( auto& Container, Drop Amount ) { return ForwardRange{ PartitionLine( Container, Amount ), Container.end() }; }

    constexpr auto operator|( auto& Container, Take Amount ) { return ForwardRange{ Container.begin(), PartitionLine( Container, Amount ) }; }

}  // namespace IndexRange

#undef FWD_
#endif
