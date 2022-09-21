#ifndef CUSTOM_DEBUG_H
#define CUSTOM_DEBUG_H

#include <cstdio>
#include <iostream>

//#define Dump( ... ) std::cout << "Line " << __LINE__ << " : " #__VA_ARGS__ " | " << (unsigned long long)( __VA_ARGS__ ) << '\n'

namespace
{
    template<typename SizeType>
    SizeType CountDigit( SizeType n )
    {
        if( n < 0 ) return CountDigit( -n ) + 1;
        SizeType ceiling = 10;
        for( SizeType i = 0; ++i < 20; ceiling *= 10 )
            if( n < ceiling ) return i;
        return static_cast<SizeType>( 20 );
    }

    template<typename T>
    void DumpBinary( const T& src, int bytes = 1 )
    {
        bytes *= sizeof( T );

        constexpr int ColumnCount = 8;

        if( bytes > ColumnCount && bytes % ColumnCount > 0 )
            for( int padding = ColumnCount - bytes % ColumnCount; padding-- > 0; ) printf( "         " );

        auto Byte = reinterpret_cast<const unsigned char*>( &src );
        for( int i = bytes; i-- > 0; )
        {
            for( int bit = 8; bit-- > 0; ) putchar( Byte[i] >> bit & 1 ? '1' : 'o' );
            putchar( ' ' );
            if( i % ColumnCount == 0 ) printf( "[%3u ]\n", i );
        }
        putchar( '\n' );
    }


}  // namespace

struct TypeOf{
    template<typename Type>
    void operator,( Type&& )
    {
        return +[/* Read Error Message */] {};
    }
};
#define Use_this_operator_to_check_object_type TypeOf{},
#define TypeOf Use_this_operator_to_check_object_type


#ifdef TEST_CODE
#include <vector>

int main()
{
    auto vec = std::vector<int>{ 1, 2, 3, 4, 5 };
    auto rbegin = vec.rbegin();

    DumpBinary( vec );

    TypeOf rbegin ;
    return 0;
}

#endif

#endif