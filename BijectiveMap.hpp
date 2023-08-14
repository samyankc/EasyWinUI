#ifndef H_BIJECTIVE_MAP_
#define H_BIJECTIVE_MAP_

#include <array>
#include <utility>
#include <algorithm>

template<typename KeyType, typename ValueType, std::size_t N = 0>
struct BijectiveMap : std::array<std::pair<KeyType, ValueType>, N>
{
    constexpr auto operator[]( const KeyType& TargetKey ) const
    {
        for( auto&& Node : *this )
            if( Node.first == TargetKey ) return Node.second;
        return ValueType{};
    }

    constexpr auto Inverse() const
    {
        auto InverseMap = BijectiveMap<ValueType, KeyType, N>{};
        std::transform( this->begin(), this->end(), InverseMap.begin(), []( auto& Node ) {
            return std::pair<ValueType, KeyType>{ Node.second, Node.first };
        } );
        return InverseMap;
    }
};

template<typename KeyType, typename ValueType>
struct BijectiveMap<KeyType, ValueType, 0>
{
    using PairType = std::pair<KeyType, ValueType>;

    template<PairType... Nodes>
    constexpr static auto Init()
    {
        return BijectiveMap<KeyType, ValueType, sizeof...( Nodes )>{ Nodes... };
    }
};

#endif