#ifndef INDEXRANGE_H
#define INDEXRANGE_H

#include <type_traits>

namespace IndexRange
{
    template<typename T>
    struct SelfReferencing
    {
        constexpr SelfReferencing( T src ) : self_{ src } {}
        constexpr T operator*() { return self_; }
        constexpr operator T&() { return self_; }
        constexpr operator const T&() const { return self_; }

      protected:
        T self_;
    };

    template<typename Iter, typename CRTP>
    struct IterOperator
    {
        constexpr IterOperator( Iter src ) : current_( src ) {}
        constexpr decltype( auto ) operator*() { return *static_cast<CRTP&>( *this ).current_; }
        constexpr bool operator!=( const CRTP& rhs ) const
        {
            return static_cast<const CRTP&>( *this ).current_ != rhs.current_;
        }
        constexpr auto operator++() { return static_cast<CRTP&>( *this ) += +1; }
        constexpr auto operator--() { return static_cast<CRTP&>( *this ) += -1; }

      protected:
        Iter current_;
    };

    template<typename Iter>
    struct ForwardIter : IterOperator<Iter, ForwardIter<Iter>>
    {
        constexpr ForwardIter( Iter src ) : IterOperator<Iter, ForwardIter<Iter>>( src ) {}
        constexpr auto operator+=( auto n ) { return this->current_ += n, *this; }
    };

    template<typename Iter>
    struct ReverseIter : IterOperator<Iter, ReverseIter<Iter>>
    {
        constexpr ReverseIter( Iter src ) : IterOperator<Iter, ReverseIter<Iter>>( --src ) {}
        constexpr auto operator+=( auto n ) { return this->current_ += -n, *this; }
    };

    template<typename Iter>
    struct ForwardRange
    {
        constexpr ForwardRange( Iter begin__, Iter end__ ) : Begin( begin__ ), End( end__ ) {}
        constexpr auto begin() const { return Begin; }
        constexpr auto end() const { return End; }

      protected:
        Iter Begin;
        Iter End;
    };

    template<typename Iter>
    using RangeTemplate = ForwardRange<Iter>;

    template<typename Iter>
    struct ReverseRange : RangeTemplate<ReverseIter<Iter>>
    {
        constexpr ReverseRange( Iter begin__, Iter end__ ) : RangeTemplate<ReverseIter<Iter>>( end__, begin__ ) {}
    };

    // using IndexIter = ForwardIter<SelfReferencing<long long> >;
    template<typename T, typename Iter = ForwardIter<SelfReferencing<T>>>
    struct Range : RangeTemplate<Iter>
    {
        constexpr Range( T first, T last ) : RangeTemplate<Iter>( Iter{ first }, Iter{ last + 1 } ) {}
        constexpr Range( T distance ) : Range( T{ 0 }, distance - 1 ) {}
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

    template<typename Container, typename Adaptor>
    requires std::is_same_v<Adaptor, Drop> ||  //
             std::is_same_v<Adaptor, Take> ||  //
             std::is_same_v<Adaptor, struct Reverse>
    constexpr auto operator|( Container&& C, Adaptor A )
    {
        return C | A;
    }

    constexpr auto operator|( auto& Container, struct Reverse )
    {
        return ReverseRange{ Container.begin(), Container.end() };
    }

    constexpr auto PartitionLine( auto& Container, auto Amount )
    {
        auto NewBegin = Container.begin();
        auto End = Container.end();
        for( [[maybe_unused]] auto i : Range( Amount.Count ) )
            if( NewBegin != End ) ++NewBegin;
        return NewBegin;
    }

    constexpr auto operator|( auto& Container, Drop Amount )
    {
        return ForwardRange{ PartitionLine( Container, Amount ), Container.end() };
    }

    constexpr auto operator|( auto& Container, Take Amount )
    {
        return ForwardRange{ Container.begin(), PartitionLine( Container, Amount ) };
    }

}  // namespace IndexRange

#endif
