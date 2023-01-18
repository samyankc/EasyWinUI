#ifndef INDEXRANGE_H
#define INDEXRANGE_H

#include <type_traits>

namespace index_range {
    template<typename T>
    struct SelfReferencing
    {
        T self_;
        constexpr SelfReferencing( T src ) : self_{ src } {}
        constexpr T operator*() { return self_; }
        constexpr operator T&() { return self_; }
        constexpr operator const T&() const { return self_; }
    };

    template<typename Iter, typename CRTP>
    struct IterOperator
    {
        Iter current_;
        constexpr IterOperator( Iter src ) : current_( src ) {}
        constexpr decltype( auto ) operator*() { return *static_cast<CRTP&>( *this ).current_; }
        constexpr bool operator!=( const CRTP& rhs ) const { return static_cast<const CRTP&>( *this ).current_ != rhs.current_; }
        constexpr auto operator++() { return static_cast<CRTP&>( *this ) += +1; }
        constexpr auto operator--() { return static_cast<CRTP&>( *this ) += -1; }
    };

    template<typename Iter>
    struct ForwardIter : IterOperator<Iter, ForwardIter<Iter> >
    {
        constexpr ForwardIter( Iter src ) : IterOperator<Iter, ForwardIter<Iter> >( src ) {}
        constexpr auto operator+=( auto n ) { return this->current_ += n, *this; }
    };

    template<typename Iter>
    struct ReverseIter : IterOperator<Iter, ReverseIter<Iter> >
    {
        constexpr ReverseIter( Iter src ) : IterOperator<Iter, ReverseIter<Iter> >( --src ) {}
        constexpr auto operator+=( auto n ) { return this->current_ += -n, *this; }
    };

    template<typename Iter>
    struct RangeTemplate
    {
        Iter begin_;
        Iter end_;
        constexpr RangeTemplate( Iter begin__, Iter end__ ) : begin_( begin__ ), end_( end__ ) {}
        constexpr auto begin() const { return begin_; }
        constexpr auto end() const { return end_; }
    };

    template<typename Iter>
    using ForwardRange = RangeTemplate<Iter>;

    template<typename Iter>
    ForwardRange<Iter> MakeForwardRange( Iter begin, Iter end )
    {
        return RangeTemplate{ begin, end };
    }

    template<typename Iter>
    struct ReverseRange : RangeTemplate<ReverseIter<Iter> >
    {
        constexpr ReverseRange( Iter begin__, Iter end__ ) : RangeTemplate<ReverseIter<Iter> >( end__, begin__ ) {}
    };



    // using IndexIter = ForwardIter<SelfReferencing<long long> >;
    template<typename T, typename Iter = ForwardIter<SelfReferencing<T> > >
    struct Range : RangeTemplate<Iter>
    {
        constexpr Range( T first, T last ) : RangeTemplate<Iter>( Iter{ first }, Iter{ last + 1 } ) {}
        constexpr Range( T distance ) : Range( 0, distance - 1 ) {}
    };

    struct Reverse
    {};
    struct Drop
    {
        long long N = 0;
    };
    struct Take
    {
        long long N = 0;
    };

    template<typename Container, typename Adaptor, std::enable_if_t<std::is_same_v<Adaptor, Drop> || std::is_same_v<Adaptor, Take> || std::is_same_v<Adaptor, Reverse>, bool> = true>
    constexpr auto operator|( Container&& C, Adaptor A )
    {
        return C | A;
    }

    constexpr auto operator|( auto& Container, Reverse ) { return ReverseRange( Container.begin(), Container.end() ); }

    constexpr auto operator|( auto& Container, Drop Amount )
    {
        auto new_begin_ = Container.begin();
        auto end_       = Container.end();
        for( auto i : Range( Amount.N ) )
            if( new_begin_ != end_ ) ++new_begin_;
        return MakeForwardRange( new_begin_, end_ );
    }

    constexpr auto operator|( auto& Container, Take Amount )
    {
        auto new_begin_ = Container.begin();
        auto end_       = Container.end();
        for( auto i : Range( Amount.N ) )
            if( new_begin_ != end_ ) ++new_begin_;
        return MakeForwardRange( Container.begin(), new_begin_ );
    }

}  // namespace index_range

#endif

