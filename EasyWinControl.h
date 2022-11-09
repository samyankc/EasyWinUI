/*

Usage:
auto Control = CreateControl("Window Class Name","Control Class Name");

Control.Click(x,y);
Control.Drag({x1,y2},{x2,y2});
COLORREF colour = Control.GetColour(x,y);

*/
#include <type_traits>
#define SHOWACTION

#ifndef EASYWINCONTROL_H
#define EASYWINCONTROL_H

//#include <EasyNotepad.h>
#include <cstdint>
#include <windows.h>
#include <gdiplus.h>

#include <algorithm>
#include <iterator>
#include <ratio>
#include <bit>
#include <string_view>

#include <cstring>
#include <iomanip>
#include <ios>
#include <iostream>
#include <map>
#include <span>
#include <string>
#include <utility>
#include <vector>
#include <windef.h>
#include <wingdi.h>
#include <winnt.h>


#define SIMILARITY_THRESHOLD 20

#define STRINGIFY_IMPL( s ) #s
#define STRINGIFY( s ) STRINGIFY_IMPL( s )

// #define IGNORE_COLOUR ( RGBQUAD{ .rgbReserved = 1 } )
// #define REJECT_COLOUR ( RGBQUAD{ .rgbReserved = 2 } )
// EXCLUDE_COLOUR | 0xBBGGRR  ==>  all colours allowed except 0xBBGGRR ( exact match )

//#define IGNORE_TEXT         "_WwWwWwW_"
// #define IGNORE_TEXT STRINGIFY( IGNORE_COLOUR )
// #define _WwWwW_ IGNORE_COLOUR
// #define _WwWwWwW_ IGNORE_COLOUR

constexpr auto IGNORE_PIXEL = decltype( std::declval<RGBQUAD>().rgbReserved ){ 1 };
constexpr auto REJECT_COLOUR = decltype( std::declval<RGBQUAD>().rgbReserved ){ 2 };
constexpr auto IGNORE_TEXT = "_WwWwWwW_";
//#define Associate(Object,Method) auto Method = std::bind_front(&decltype(Object)::Method, &Object)
/*
#define Associate(Object,Method)    \
auto Method = [&Object = Object]    \
<typename... Ts>(Ts&&... Args)      \
{ return Object.Method( std::forward<Ts>(Args)...); }
*/
constexpr auto nMaxCount = 256uz;

inline auto ShowHandleName( HWND Handle )
{
    char TextBuffer[nMaxCount];

    std::cout << std::left << std::setw( 13 )  //
              << std::hex << std::showbase << ( Handle ) << std::dec;

    GetClassName( Handle, TextBuffer, nMaxCount );
    std::cout << std::setw( 32 ) << std::string( "[ " ) + TextBuffer + " ]" << ' ';

    GetWindowText( Handle, TextBuffer, nMaxCount );
    std::cout << "[ " << TextBuffer << " ] \n";
}

inline void ShowAllChild( HWND Handle )
{
    EnumChildWindows( Handle,
                      []( HWND ChildHandle, LPARAM ) -> WINBOOL {
                          std::cout << "\\ ";
                          ShowHandleName( ChildHandle );
                          return true;
                      },
                      {} );
}

inline HWND ObtainFocusHandle( DWORD Delay = 2000 )
{
    Sleep( Delay );
    if( HWND ret = GetFocus(); ret ) return ret;
    return GetForegroundWindow();
}

constexpr auto operator"" _VK( char ch )
{
    if( ch >= 'a' && ch <= 'z' ) ch -= 'a' - 'A';
    return static_cast<WPARAM>( ch );
}

inline auto GetWindowHandleAll()
{
    std::vector<HWND> Handles;
    Handles.reserve( 256 );

    EnumWindows(
        []( HWND Handle, LPARAM lParam_ ) -> WINBOOL {
            reinterpret_cast<std::vector<HWND>*>( lParam_ )->push_back( Handle );
            return true;
        },
        reinterpret_cast<LPARAM>( &Handles ) );

    return Handles;
}

inline auto GetWindowHandleByName( std::string_view Name )
{
    auto Handles = GetWindowHandleAll();
    auto NotFoundBy = [=]( auto... NameExtractors ) {
        return [=]( HWND Handle ) {
            auto FoundBy = [=]( auto NameExtractor ) {
                char TextBuffer[nMaxCount];
                NameExtractor( Handle, TextBuffer, nMaxCount );
                return std::string_view( TextBuffer ).contains( Name );
            };
            return ! ( FoundBy( NameExtractors ) || ... );
        };
    };
    std::erase_if( Handles, NotFoundBy( GetClassName, GetWindowText ) );
    return Handles;
}

inline auto GetWindowHandleByName_( std::string_view Name )
{
    auto Handles = GetWindowHandleAll();
    auto Results = decltype( Handles ){};
    std::ranges::copy_if( Handles, std::back_inserter( Results ),  //
                          [=]( HWND Handle ) {
                              auto FoundBy = [=]( auto NameExtractor ) {
                                  char TextBuffer[nMaxCount];
                                  NameExtractor( Handle, TextBuffer, nMaxCount );
                                  return std::string_view( TextBuffer ).contains( Name );
                              };
                              return FoundBy( GetClassName ) || FoundBy( GetWindowText );
                          } );
    return Results;
}

[[deprecated( "use alternative overload version" )]]  //
inline bool
Similar( int LHS, int RHS, int SimilarityThreshold = SIMILARITY_THRESHOLD )
{
    constexpr auto Delta = []( int x, int y ) { return ( x > y ) ? ( x - y ) : ( y - x ); };
    return Delta( GetRValue( LHS ), GetRValue( RHS ) ) <= SimilarityThreshold &&
           Delta( GetGValue( LHS ), GetGValue( RHS ) ) <= SimilarityThreshold &&
           Delta( GetBValue( LHS ), GetBValue( RHS ) ) <= SimilarityThreshold;
}

inline bool Similar( RGBQUAD LHS, RGBQUAD RHS, int SimilarityThreshold = SIMILARITY_THRESHOLD )
{
    constexpr auto Delta = []( BYTE x, BYTE y ) { return ( x > y ) ? ( x - y ) : ( y - x ); };
    return Delta( LHS.rgbBlue, RHS.rgbBlue ) <= SimilarityThreshold &&
           Delta( LHS.rgbGreen, RHS.rgbGreen ) <= SimilarityThreshold &&
           Delta( LHS.rgbRed, RHS.rgbRed ) <= SimilarityThreshold;
}

// struct Point
// {
//     LPARAM POINT_DATA;
//     constexpr Point( LPARAM _src ) : POINT_DATA( _src ) {}
//     constexpr Point( int x, int y ) : POINT_DATA( MAKELPARAM( x, y ) ) {}
//     constexpr         operator LPARAM&() { return POINT_DATA; }
//     constexpr Point   operator-() const { return { -x(), -y() }; }
//     constexpr Point   operator<<=( const Point& _offset ) const { return { x() + _offset.x(), y() + _offset.y() }; }
//     constexpr Point&  operator+=( const Point& _offset ) { return *this = { x() + _offset.x(), y() + _offset.y() }; }
//     constexpr int16_t x() const { return LOWORD( POINT_DATA ); }
//     constexpr int16_t y() const { return HIWORD( POINT_DATA ); }
//     constexpr friend bool operator==( const Point& lhs, const Point& rhs ) { return lhs.POINT_DATA == rhs.POINT_DATA; }
// };
// using Offset = Point;

constexpr auto operator+( const POINT& LHS, const POINT& RHS ) { return POINT{ LHS.x + RHS.x, LHS.y + RHS.y }; }
constexpr auto operator-( const POINT& LHS, const POINT& RHS ) { return POINT{ LHS.x - RHS.x, LHS.y - RHS.y }; }
constexpr auto operator==( const POINT& LHS, const POINT& RHS ) { return LHS.x == RHS.x && LHS.y == RHS.y; }
constexpr auto operator==( const SIZE& LHS, const SIZE& RHS ) { return LHS.cx == RHS.cx && LHS.cy == RHS.cy; }

constexpr auto& operator+=( POINT& LHS, const SIZE& RHS )
{
    LHS.x += RHS.cx;
    LHS.y += RHS.cy;
    return LHS;
}

constexpr auto& operator-=( POINT& LHS, const SIZE& RHS )
{
    LHS.x -= RHS.cx;
    LHS.y -= RHS.cy;
    return LHS;
}

constexpr auto Int_To_RGBQUAD( unsigned int n ) noexcept { return std::bit_cast<RGBQUAD>( n ); }

constexpr auto RGBQUAD_To_Int( RGBQUAD c ) noexcept { return std::bit_cast<unsigned int>( c ); }

constexpr auto operator|( RGBQUAD LHS, RGBQUAD RHS ) noexcept
{
    return Int_To_RGBQUAD( RGBQUAD_To_Int( LHS ) | RGBQUAD_To_Int( RHS ) );
}

constexpr auto operator&( RGBQUAD LHS, RGBQUAD RHS ) noexcept
{
    return Int_To_RGBQUAD( RGBQUAD_To_Int( LHS ) & RGBQUAD_To_Int( RHS ) );
}

constexpr auto operator==( RGBQUAD LHS, RGBQUAD RHS ) noexcept
{
    return RGBQUAD_To_Int( LHS ) == RGBQUAD_To_Int( RHS );
}

struct EasyBitMap
{
    POINT                Origin;
    SIZE                 Dimension;
    BITMAPINFO           BitMapInformation{};
    std::vector<RGBQUAD> Pixels;

    EasyBitMap() = default;
    EasyBitMap( POINT Origin_, SIZE Dimension_ ) : Origin( Origin_ ), Dimension( Dimension_ )
    {
        BitMapInformation.bmiHeader.biSize = sizeof( BITMAPINFOHEADER );
        BitMapInformation.bmiHeader.biWidth = Dimension.cx;
        BitMapInformation.bmiHeader.biHeight = -Dimension.cy;
        BitMapInformation.bmiHeader.biPlanes = 1;
        BitMapInformation.bmiHeader.biBitCount = 32;
        BitMapInformation.bmiHeader.biSizeImage = 0;
        BitMapInformation.bmiHeader.biCompression = BI_RGB;

        Pixels.resize( Dimension.cx * Dimension.cy );
    }

    auto IsolateColour( RGBQUAD KeepColour, int SimilarityThreshold = SIMILARITY_THRESHOLD ) const
    {
        EasyBitMap NewMap{ *this };
        for( auto&& CurrentPixel : NewMap.Pixels )
            if( ! Similar( CurrentPixel, KeepColour, SimilarityThreshold ) )
            {
                CurrentPixel = { .rgbReserved = IGNORE_PIXEL };
            }
        // else CurrentPixel = KeepColour;
        return NewMap;
    }

    auto ExtractByColour( RGBQUAD TargetColour, int SimilarityThreshold = SIMILARITY_THRESHOLD ) const
    {
        EasyBitMap NewMap{ *this };
        for( auto&& CurrentPixel : NewMap.Pixels )
            if( ! Similar( CurrentPixel, TargetColour, SimilarityThreshold ) )
                CurrentPixel = { .rgbReserved = IGNORE_PIXEL };
        return NewMap;
    }

    auto GetColour( int x, int y ) { return Pixels[x + y * Dimension.cx]; }

    auto& operator[]( const POINT& CheckPoint ) { return Pixels[CheckPoint.x + CheckPoint.y * Dimension.cx]; }
    //auto& operator[]( const LONG x, const LONG y ) { return Pixels[x + y * Dimension.cx]; }

    // SIZE FindMonochromeBlockLocal( const EasyBitMap& MonochromeMap, const POINT prelim, const POINT sentinel )
    // {
    //     if( prelim.x >= sentinel.x || prelim.y >= sentinel.y ) return Dimension;

    //     auto Match = MonochromeMap.Pixels[0];
    //     auto w = MonochromeMap.Dimension.cx;
    //     auto h = MonochromeMap.Dimension.cy;

    //     auto BoundLeft = prelim.x;
    //     for( const auto limit = prelim.x - w + 1; BoundLeft-- > limit && GetColour( BoundLeft, prelim.y ) == Match; )
    //     {}

    //     auto BoundTop = prelim.y;
    //     for( const auto limit = prelim.y - h + 1; BoundTop-- > limit && GetColour( prelim.x, BoundTop ) == Match; )
    //     {}

    //     auto BoundRight = prelim.x;
    //     for( const auto limit = sentinel.x - 1; BoundRight++ < limit && GetColour( BoundRight, prelim.y ) == Match; )
    //     {}

    //     auto BoundBottom = prelim.y;
    //     for( const auto limit = sentinel.y - 1; BoundBottom++ < limit && GetColour( prelim.x, BoundBottom ) == Match; )
    //     {}

    //     POINT new_prelim = { BoundLeft + w, BoundTop + h };
    //     POINT new_sentinel = { BoundRight, BoundBottom };

    //     if( new_prelim != prelim )
    //         return FindMonochromeBlockLocal( MonochromeMap, new_prelim, new_sentinel );  // search in shrinked region
    //     else
    //     {
    //         for( auto y = prelim.y; y-- > BoundTop + 1; )
    //             for( auto x = prelim.x; x-- > BoundLeft + 1; )
    //                 if( GetColour( x, y ) != Match )
    //                 {
    //                     // region #1: { prelim.x(), y+h }, new_sentinel ; everything below y
    //                     auto LocalSearch = FindMonochromeBlockLocal( MonochromeMap, { prelim.x, y + h }, new_sentinel );
    //                     if( LocalSearch != Dimension ) return LocalSearch;

    //                     // region #2: { x+w, prelim.y() }, { new_sentinel.x(), y+h-1 } ; to the right, exclude region #1
    //                     // overlap
    //                     return FindMonochromeBlockLocal( MonochromeMap, { x + w, prelim.y },
    //                                                      { new_sentinel.x, y + h } );
    //                 }
    //         return { BoundLeft + 1, BoundTop + 1 };  // found
    //     }
    // }

    // SIZE FindMonochromeBlock( const EasyBitMap& MonochromeMap )
    // // special use of ControlBitMap format, only use Dimension field & single element Pixels vector
    // // eg. { {0,0},{102,140},{0x0F7F7F7} }
    // {
    //     auto Match = MonochromeMap.Pixels[0];
    //     auto w = MonochromeMap.Dimension.cx;
    //     auto h = MonochromeMap.Dimension.cy;

    //     // disect entire region into sub regions
    //     // each preliminary check point separate w-1/h-1 apart => x_n + w = x_n+1
    //     for( auto prelim_y = h - 1; prelim_y < Dimension.cy; prelim_y += h )
    //         for( auto prelim_x = w - 1; prelim_x < Dimension.cx; prelim_x += w )
    //         {
    //             if( GetColour( prelim_x, prelim_y ) != Match ) continue;
    //             // main search starts here
    //             auto LocalSearch =
    //                 FindMonochromeBlockLocal( MonochromeMap, { prelim_x, prelim_y },
    //                                           { prelim_x + w < Dimension.cx ? prelim_x + w : Dimension.cx,
    //                                             prelim_y + h < Dimension.cy ? prelim_y + h : Dimension.cy } );
    //             if( LocalSearch != Dimension ) return LocalSearch;
    //         }

    //     return Dimension;  // indicate not found
    // }

    void DisplayAt( HWND CanvasHandle, int OffsetX = 0, int OffsetY = 0 )
    {
        if( CanvasHandle == NULL ) return;
        HDC hdc = GetDC( CanvasHandle );
        SetDIBitsToDevice( hdc, OffsetX, OffsetY, Dimension.cx, Dimension.cy,  //
                           0, 0, 0, Dimension.cy, Pixels.data(), &BitMapInformation, DIB_RGB_COLORS );
        ReleaseDC( CanvasHandle, hdc );
    }

    void DisplayAt( int OffsetX, int OffsetY ) { DisplayAt( GetConsoleWindow(), OffsetX, OffsetY ); }

    std::string Code()
    {
        std::size_t        x = Origin.x, y = Origin.y, w = Dimension.cx, h = Dimension.cy;
        std::ostringstream OSS;
        OSS << "{";
        OSS << "\n    {" << x << "," << y << "}, {" << w << "," << h << "},";
        OSS << "\n    {";
        for( auto pos = 0uz, y_ = 0uz; y_ < h; ++y_ )
        {
            for( auto x_ = 0uz; x_ < w; ++x_, ++pos )
            {
                if( x_ % w == 0 ) OSS << "\n    ";

                if( Pixels[pos].rgbReserved == IGNORE_PIXEL )
                    OSS << IGNORE_TEXT;
                else
                    OSS << "0x" << std::uppercase << std::setfill( '0' ) << std::setw( 7 ) << std::hex
                        << RGBQUAD_To_Int( Pixels[pos] );

                if( pos < w * h - 1 ) OSS << ",";
            }
        }
        OSS << "\n    }";
        OSS << "\n};\n";

        return OSS.str();
    }
};

//#define AcquireFocus(_Handle_) AcquireFocus_ TemporaryFocus(_Handle_); auto dummy:{0}
// #define AcquireFocus( _Handle_ )                         \
//     struct                                               \
//     {                                                    \
//         const AcquireFocus_& _Dummy_;                    \
//         bool                 _InLoop_;                   \
//     } TemporaryFocus{ AcquireFocus_{ _Handle_ }, true }; \
//     TemporaryFocus._InLoop_;                             \
//     TemporaryFocus._InLoop_ = false

// struct AcquireFocus_
// {
//     HWND LastForegroundWindow;

//     inline void BringToForeground( HWND Handle )
//     {
//         SendMessage( Handle, WM_ACTIVATE, WA_CLICKACTIVE, 0 );
//         SetForegroundWindow( Handle );
//         // SwitchToThisWindow(Handle,true);
//         // Sleep(100);
//     }

//     AcquireFocus_( HWND Handle )
//     {
//         LastForegroundWindow = GetForegroundWindow();
//         BringToForeground( Handle );
//     }

//     ~AcquireFocus_() { BringToForeground( LastForegroundWindow ); }
// };

struct VirtualDeviceContext
{
    HWND    Handle;
    HDC     Destination{}, Source;
    HBITMAP BitMap{};

    VirtualDeviceContext( HWND Handle_, SIZE Dimension ) : Handle{ Handle_ }
    {
        Source = GetDC( Handle );
        if( Dimension == SIZE{ 0, 0 } ) return;
        Destination = CreateCompatibleDC( Source );
        BitMap = CreateCompatibleBitmap( Source, Dimension.cx, Dimension.cy );
        SelectObject( Destination, BitMap );
    }

    ~VirtualDeviceContext()
    {
        if( BitMap ) DeleteObject( BitMap );
        if( Destination ) DeleteDC( Destination );
        ReleaseDC( Handle, Source );
    }

    operator HDC() const noexcept { return Source; }
};

struct EasyControl
{
    HWND  Handle;
    DWORD ClickDuration{ 30 };
    DWORD ClickDelay{ 120 };
    DWORD KeyDuration{ 20 };
    DWORD KeyDelay{ 80 };

    EasyControl() : Handle( NULL ) {}

    EasyControl( HWND _hwnd ) : Handle( _hwnd ) {}

    EasyControl( const EasyControl& _Control ) = default;

    EasyControl( std::string_view WindowName, std::string_view ControlName )
    {
        auto WindowHandles = GetWindowHandleByName( WindowName );

#ifdef SHOWNAME
        for( auto& W : WindowHandles )
        {
            ShowHandleName( W );
            ShowAllChild( W );
            std::cout << '\n';
        }
#endif

        switch( WindowHandles.size() )
        {
            case 0 : Handle = NULL; return;
            case 1 :
                Handle = FindWindowEx( WindowHandles[0], NULL, ControlName.data(), NULL );
                if( Handle == NULL ) Handle = WindowHandles[0];
                break;
            default :
            {
                for( std::size_t i = 0; i < WindowHandles.size(); ++i )
                {
                    std::cout << "[" << i << "] ";
                    ShowHandleName( WindowHandles[i] );
                }
                std::cout << "Pick handle: ";
                int choice;
                std::cin >> choice;
                std::cout << "Chosen: ";
                ShowHandleName( WindowHandles[choice] );
                Handle = FindWindowEx( WindowHandles[choice], NULL, ControlName.data(), NULL );
                break;
            }
        }
    }

    operator HWND() const { return Handle; }

    auto empty() const noexcept { return Handle == NULL; }

    inline void SendKey( WPARAM VirtualKey, LPARAM HardwareScanCode = 1 )
    {
        SendMessage( Handle, WM_KEYDOWN, VirtualKey, HardwareScanCode );
        Sleep( KeyDuration );
        SendMessage( Handle, WM_KEYUP, VirtualKey, HardwareScanCode );
        Sleep( KeyDelay );
    }

    inline void SendChar( WPARAM CharacterCode )
    {
        SendMessage( Handle, WM_CHAR, CharacterCode, 0 );
        Sleep( KeyDelay );
    }

    inline void SendText( const char* Text ) { SendMessage( Handle, WM_SETTEXT, 0, reinterpret_cast<LPARAM>( Text ) ); }

    SIZE GetClientDimension()
    {
        RECT ClientRect;
        GetClientRect( Handle, &ClientRect );
        return { ClientRect.right, ClientRect.bottom };
    }

    void TranslatePOINT( POINT& Origin )
    {
        static const auto ClientWidth = GetClientDimension().cx;
        static const auto ClientHeight = GetClientDimension().cy;
        if( Origin.x >= 0 && Origin.y >= 0 ) return;

        Origin = POINT{ Origin.x + ( Origin.x < 0 ) * ClientWidth, Origin.y + ( Origin.y < 0 ) * ClientHeight };
    }

    POINT TranslatePOINT( POINT&& Origin )
    {
        TranslatePOINT( Origin );
        return Origin;
    }

    void SendMouseInput( double PercentX, double PercentY )
    {
        static const auto ScreenWidth = GetSystemMetrics( SM_CXSCREEN );
        static const auto ScreenHeight = GetSystemMetrics( SM_CYSCREEN );
        static const auto ClientWidth = GetClientDimension().cx;
        static const auto ClientHeight = GetClientDimension().cy;
        // thread safe initialisation may cause performance issue
        // need modification

        auto ClientPoint = POINT{ .x = long( PercentX * ClientWidth ), .y = long( PercentY * ClientHeight ) };
        if( ClientToScreen( Handle, &ClientPoint ) )
        {
            mouse_event( MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_MOVE | MOUSEEVENTF_LEFTUP,
                         ClientPoint.x * 65535 / ScreenWidth, ClientPoint.y * 65535 / ScreenHeight, 0, 0 );
            Sleep( 100 );

            SendMessage( Handle, WM_RBUTTONDOWN, 0, 0 );
            Sleep( ClickDuration );
            SendMessage( Handle, WM_RBUTTONUP, 0, 0 );
            Sleep( ClickDelay );

            // Click(0);
        }
        else
            std::cout << "\n"
                      << "SendMouseInput to ( 0." << PercentX << " full width, 0." << PercentY
                      << " full height ) failed.\n";
    }

    inline auto MouseLeftEvent( UINT Msg, POINT Point ) const
    {
        return SendMessage( Handle, Msg, MK_LBUTTON, MAKELPARAM( Point.x, Point.y ) );
    }
    inline auto MouseLeftDown( POINT Point ) const { return MouseLeftEvent( WM_LBUTTONDOWN, Point ); }
    inline auto MouseLeftMove( POINT Point ) const { return MouseLeftEvent( WM_MOUSEMOVE, Point ); }
    inline auto MouseLeftUp( POINT Point ) const { return MouseLeftEvent( WM_LBUTTONUP, Point ); }


    inline void Click( POINT Point, unsigned Repeat = 1 ) const
    {
#ifdef SHOWACTION
        std::cout << "[ Click (" << Point.x << ", " << Point.y << ") ]" << std::endl;
#endif
        while( Repeat-- )
        {
            MouseLeftDown( Point );
            Sleep( ClickDuration );
            MouseLeftUp( Point );
            Sleep( ClickDelay );
        }
    }
    inline void Click( int x, int y, unsigned Repeat = 1 ) const { Click( POINT{ x, y }, Repeat ); }

    inline void Drag( POINT POINT_1, POINT POINT_2, bool SmoothDrag = true ) const
    {
#ifdef SHOWACTION
        std::cout << "[ Drag (" << POINT_1.x << ", " << POINT_1.y << ")"
                  << "    To (" << POINT_2.x << ", " << POINT_2.y << ") ]" << std::endl;
#endif

        MouseLeftDown( POINT_1 );
        Sleep( ClickDelay );

        if( SmoothDrag )
        {
            constexpr auto Displacement = 8;

            auto IntervalCount = std::max( std::abs( ( POINT_2.x - POINT_1.x ) / Displacement ),  //
                                           std::abs( ( POINT_2.y - POINT_1.y ) / Displacement ) );
            auto Interval = SIZE{ ( POINT_2.x - POINT_1.x ) / IntervalCount,  //
                                  ( POINT_2.y - POINT_1.y ) / IntervalCount };

            auto CurrentPoint = POINT_1;
            for( auto i = 0; i < IntervalCount; ++i )
            {
                MouseLeftMove( CurrentPoint );
                CurrentPoint += Interval;
#ifdef SHOWACTION
                std::cout << "    To (" << CurrentPoint.x << ", " << CurrentPoint.y << ")" << std::endl;
#endif
                Sleep( 10 );
            }
        }
        MouseLeftMove( POINT_2 );
        Sleep( ClickDelay );
        MouseLeftUp( POINT_2 );
    }

    EasyBitMap CaptureRegion( POINT Origin, SIZE Dimension ) const
    {
        auto BitMap = EasyBitMap( Origin, Dimension );

        if( Dimension == SIZE{ 0, 0 } ) return BitMap;

        auto VirtualDC = VirtualDeviceContext( Handle, Dimension );

        auto [x, y] = Origin;
        auto [w, h] = Dimension;
        BitBlt( VirtualDC.Destination, 0, 0, w, h, VirtualDC.Source, x, y, SRCCOPY );

        GetDIBits( VirtualDC.Destination, VirtualDC.BitMap, 0, h,  //
                   BitMap.Pixels.data(), &BitMap.BitMapInformation, DIB_RGB_COLORS );

        return BitMap;
    }

    inline bool MatchBitMap( const EasyBitMap& ReferenceBitMap, int SimilarityThreshold = SIMILARITY_THRESHOLD )
    {
        // need complete re-work
        // capture whole image than compare
        (void)SimilarityThreshold;

        auto VirtualDC = VirtualDeviceContext( Handle, ReferenceBitMap.Dimension );
        auto [x, y] = ReferenceBitMap.Origin;
        auto [w, h] = ReferenceBitMap.Dimension;
        BitBlt( VirtualDC.Destination, 0, 0, w, h, VirtualDC.Source, x, y, SRCCOPY );

        for( auto pos{ 0 }, j{ 0 }; j < h; ++j )
            for( auto i{ 0 }; i < w; ++i, ++pos )
            {
                if( ReferenceBitMap.Pixels[pos].rgbReserved == IGNORE_PIXEL ) continue;

                // if ( ReferenceBitMap.Pixels[pos] &  REJECT_COLOUR )
                // {
                // if ( !Similar( GetPixel(VirtualDC, i, j), ReferenceBitMap.Pixels[pos], SimilarityThreshold ) )
                // continue; return false;
                // }
                // if ( !Similar( GetPixel(VirtualDC, i, j), ReferenceBitMap.Pixels[pos], SimilarityThreshold ) )
                // return false;

                //if( ( RGBQUAD_To_Int( REJECT_COLOUR & ReferenceBitMap.Pixels[pos] ) != 0 ) ==
                //   Similar( GetPixel( VirtualDC, i, j ), ReferenceBitMap.Pixels[pos], SimilarityThreshold ) )
                return false;
            }
        return true;
    }

    inline RGBQUAD GetColour( POINT Point ) { return CaptureRegion( Point, { 1, 1 } ).Pixels[0]; }

    inline RGBQUAD GetColour( LONG x, LONG y ) { return GetColour( { x, y } ); }

    void Run( void ( *Proc )( EasyControl& ) ) { Proc( *this ); }
    //#define Run(Proc) Run( (void (*)(CreateControl&)) Proc )
};

#endif
