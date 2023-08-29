#ifndef H_BIJECTIVE_MAP_
#define H_BIJECTIVE_MAP_

#include <vector>
#include <utility>

template<typename KeyType, typename ValueType>
struct BijectiveMap : std::vector<std::pair<KeyType, ValueType>>
{
    using PairType = std::pair<KeyType, ValueType>;

    constexpr auto contains( const KeyType& TargetKey ) const
    {
        for( auto&& Node : *this )
            if( Node.first == TargetKey ) return true;
        return false;
    }

    constexpr auto operator[]( const KeyType& TargetKey ) const&
    {
        for( auto&& Node : *this )
            if( Node.first == TargetKey ) return Node.second;
        return ValueType{};
    }

    constexpr auto& operator[]( const KeyType& TargetKey ) &
    {
        for( auto&& Node : *this )
            if( Node.first == TargetKey ) return Node.second;
        return this->emplace_back( TargetKey, ValueType{} ).second;
    }

    template<std::convertible_to<KeyType>... KeyTypes>
    requires( sizeof...( KeyTypes ) > 1 )
    constexpr auto operator[]( const KeyTypes&... TargetKeys ) const
    {
        return std::array{ ( *this )[TargetKeys]... };
    }

    constexpr auto Inverse() const
    {
        auto InverseMap = BijectiveMap<ValueType, KeyType>{};
        InverseMap.reserve( this->size() );
        for( auto&& Node : *this ) InverseMap.emplace_back( Node.second, Node.first );
        return InverseMap;
    }

    constexpr BijectiveMap( std::initializer_list<PairType> L )
    {
        this->reserve( L.size() );
        for( auto&& Node : L ) this->push_back( Node );
    }
};

#endif