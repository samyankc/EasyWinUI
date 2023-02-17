#ifndef MINIMAP_H
#define MINIMAP_H

#include <algorithm>
#include <vector>

namespace
{
    template<auto K>
    struct decompose_pointer_to_member
    {
        using class_type  = std::remove_cvref_t<decltype( []<typename T>( auto T::* ) -> T {}( K ) )>;
        using member_type = std::remove_cvref_t<decltype( std::declval<class_type>().*K )>;
    };

    template<auto K, typename T = typename decompose_pointer_to_member<K>::class_type>
    struct minimap : std::vector<T>
    {
        using KeyType   = typename decompose_pointer_to_member<K>::member_type;
        using ValueType = T;
        using std::vector<T>::reserve;
        using std::vector<T>::begin;
        using std::vector<T>::end;
        using std::vector<T>::emplace_back;
        using std::vector<T>::back;

        minimap( int N = 8 ) { reserve( N ); }

        bool contains_key( const KeyType& Key )
        {
            return std::any_of( begin(), end(), [ & ]( const auto& V ) { return Key == V.*K; } );
        }

        bool contains( const KeyType& Key ) { return contains_key( Key ); }

        ValueType& operator[]( const KeyType& Key )
        {
            auto Itor = std::find_if( begin(), end(), [ & ]( const auto& V ) { return Key == V.*K; } );
            if( Itor != end() ) return *Itor;
            emplace_back();
            back().*K = Key;
            return back();
        }
    };

}  // namespace


#endif


#ifdef TEST_CODE

struct S
{
    int a;
    int b;
};

int main()
{

auto m = minimap<&S::a>{};

return 0;
}


#endif
