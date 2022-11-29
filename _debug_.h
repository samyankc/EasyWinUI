#ifndef CUSTOM_DEBUG_H
#define CUSTOM_DEBUG_H

#include <cstdio>
#include <atomic>
#include <mutex>

inline void ThreadPrint( std::string_view Content, std::size_t ColumnWidth = 30 )
{
    static std::atomic<std::size_t> ThreadPrintOffsetMark{ 0 };
    thread_local std::size_t        PrintOffset = ColumnWidth * ( ThreadPrintOffsetMark++ );

    static std::mutex InternalMutex;
    std::scoped_lock  Lock( InternalMutex );

    printf( "%*s\n", PrintOffset, Content.data() );
    fflush( stdout );
}

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


template<typename Type>
void TypeOf( Type&& )
{
    return +[/* Read Error Message */] {};
}

#endif