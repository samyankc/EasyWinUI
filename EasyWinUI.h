#ifndef EASYWINUI_H
#define EASYWINUI_H

#include <Windows.h>
#include <CommCtrl.h>
#include <cstddef>
#include <gdiplus.h>

#include <algorithm>
#include <concepts>
#include <cstring>
#include <functional>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <minwindef.h>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <vector>
#include <windef.h>
#include <winnt.h>


#define EVENT_PARAMETER_LIST HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam

// helpers
namespace
{
    template<typename Derived, typename Base>
    concept DerivedFrom = std::is_base_of_v<std::decay_t<Base>, std::decay_t<Derived>>;

    template<typename TargetType, typename... CandidateTypes>
    concept MatchExactType = ( std::same_as<TargetType, CandidateTypes> || ... );

    template<typename TargetType, typename... CandidateTypes>
    concept MatchType = MatchExactType<std::decay_t<TargetType>, std::decay_t<CandidateTypes>...>;

    template<MatchType<SIZE, POINT> T>
    bool operator==( const T& LHS, const T& RHS )
    {
        return std::memcmp( &LHS, &RHS, sizeof( T ) ) == 0;
    }

    inline void PrintMSG( std::string_view PrefixString, MSG Msg )
    {
        // ignore messages
        switch( Msg.message )
        {
            case WM_MOUSEMOVE :
            case WM_SIZE :
            case WM_TIMER :
            case WM_NCMOUSEMOVE :
            case WM_NCMOUSELEAVE :
            case WM_NCLBUTTONDOWN :
            case WM_LBUTTONDOWN :
            case 96 : return;
            default : break;
        }

        //if( MonitorHandle == NULL || Msg.hwnd != MonitorHandle ) return;
        auto w10 = std::setw( 12 );
        auto w5 = std::setw( 5 );
        std::cout << PrefixString << '\t' << w10 << Msg.message                                       //
                  << w10 << Msg.wParam << w10 << HIWORD( Msg.wParam ) << w10 << LOWORD( Msg.wParam )  //
                  << w10 << Msg.lParam << w10 << HIWORD( Msg.lParam ) << w10 << LOWORD( Msg.lParam )  //
                  << std::endl;
    }

    template<std::size_t N>
    struct FixedString
    {
        char data[N];
        constexpr FixedString( const char ( &Src )[N] ) : data{}
        {
            for( std::size_t i{ 0 }; i < N; ++i ) data[i] = Src[i];
        }

        constexpr operator const char*() const { return data; }
    };

    template<std::size_t N>
    FixedString( const char ( & )[N] ) -> FixedString<N>;

    template<FixedString TargetClassName>
    bool MatchClass( HWND hwnd )
    {
        constexpr auto MaxBufferSize = 100;
        char           Buffer[MaxBufferSize]{ 0 };
        auto           ClassNameLength = GetClassName( hwnd, Buffer, MaxBufferSize );
        return strcmp( TargetClassName, Buffer ) == 0;
    }
}  // namespace

namespace EWUI
{
    int Main();

    struct Spacer
    {
        int x{};
    };

    inline struct LineBreaker
    {
    } LineBreak;

    constexpr auto EmptyAction = [] {};
    using ControlAction = std::function<void()>;
    inline std::map<HWND, ControlAction> ActionContainer;

    using ByteVector = std::vector<std::byte>;
    using CanvasContent = std::pair<BITMAPINFO, ByteVector>;
    inline std::map<HWND, CanvasContent> CanvasContainer;

    inline LRESULT CALLBACK WndProc( HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam )
    {
        switch( msg )
        {
            // case WM_CREATE : break;
            // case WM_MOVE :
            // case WM_SIZE : InvalidateRect( hwnd, NULL, true ); break;
            case WM_PAINT :
            {
                if( ! CanvasContainer.contains( hwnd ) ) return DefWindowProc( hwnd, msg, wParam, lParam );

                auto CanvasHandle = hwnd;
                auto& [BI, Pixels] = CanvasContainer[CanvasHandle];
                auto hdc = GetDC( CanvasHandle );

                SetDIBitsToDevice( hdc, 0, 0, BI.bmiHeader.biWidth, BI.bmiHeader.biHeight,  //
                                   0, 0, 0, BI.bmiHeader.biHeight, Pixels.data(), &BI, DIB_RGB_COLORS );

                ReleaseDC( CanvasHandle, hdc );
                ValidateRect( CanvasHandle, NULL );
            }
            break;
            case WM_COMMAND :
                if( ! ActionContainer.contains( reinterpret_cast<HWND>( lParam ) ) )
                    return DefWindowProc( hwnd, msg, wParam, lParam );
                std::thread( ActionContainer[reinterpret_cast<HWND>( lParam )] ).detach();
                break;
            case WM_CLOSE :
                if( GetWindow( hwnd, GW_OWNER ) == NULL )
                    DestroyWindow( hwnd );
                else
                    ShowWindow( hwnd, SW_HIDE );
                break;
            case WM_DESTROY : PostQuitMessage( 0 ); break;
            default : return DefWindowProc( hwnd, msg, wParam, lParam );
        }

        return 0;
    }

    inline struct EntryPointParamPack_
    {
        HINSTANCE hInstance;
        HINSTANCE hPrevInstance;
        LPCSTR    lpCmdLine;
        int       nCmdShow;
    } EntryPointParamPack{};

    struct BasicWindowHandle
    {
        HWND Handle{ NULL };

        auto GetRect() const noexcept
        {
            RECT rect;
            GetWindowRect( Handle, &rect );
            return rect;
        }

        auto Width() const noexcept
        {
            auto rect = GetRect();
            return rect.right - rect.left;
        }

        auto Height() const noexcept
        {
            auto rect = GetRect();
            return rect.bottom - rect.top;
        }

        auto Style() const noexcept { return GetWindowLong( Handle, GWL_STYLE ); }
        auto ExStyle() const noexcept { return GetWindowLong( Handle, GWL_EXSTYLE ); }

        auto ReSize( SIZE NewDimension ) const noexcept
        {
            SetWindowPos( Handle, HWND_NOTOPMOST, 0, 0, NewDimension.cx, NewDimension.cy,
                          SWP_SHOWWINDOW | SWP_NOMOVE | SWP_NOZORDER );
        }

        auto MoveTo( POINT NewPoint ) const noexcept
        {
            SetWindowPos( Handle, HWND_NOTOPMOST, NewPoint.x, NewPoint.y, 0, 0,
                          SWP_SHOWWINDOW | SWP_NOSIZE | SWP_NOZORDER );
        }

        auto AddStyle( DWORD NewStyle ) const noexcept
        {
            return SetWindowLong( Handle, GWL_STYLE, Style() | NewStyle );
        }

        auto RemoveStyle( DWORD TargetStyle ) const noexcept
        {
            return SetWindowLong( Handle, GWL_STYLE, Style() & ~TargetStyle );
        }

        auto AddExStyle( DWORD NewExStyle ) const noexcept
        {
            return SetWindowLong( Handle, GWL_EXSTYLE, ExStyle() | NewExStyle );
        }

        auto RemoveExStyle( DWORD TargetExStyle ) const noexcept
        {
            return SetWindowLong( Handle, GWL_EXSTYLE, ExStyle() & ~TargetExStyle );
        }
    };

    struct ControlConfiguration : BasicWindowHandle
    {
        std::optional<HWND>   Parent;
        std::optional<HMENU>  MenuID;
        std::optional<LPCSTR> ClassName;
        std::optional<LPCSTR> Label;
        std::optional<DWORD>  Style;
        std::optional<DWORD>  ExStyle;
        std::optional<POINT>  Origin;
        std::optional<SIZE>   Dimension;
    };

    constexpr auto MakeConfigurator = []<typename T>( std::optional<T> ControlConfiguration::*Field ) {
        return [Field]( T Arg ) {
            ControlConfiguration Result{};
            Result.*Field = Arg;
            return Result;
        };
    };

    constexpr auto MenuID = MakeConfigurator( &ControlConfiguration::MenuID );
    constexpr auto Label = MakeConfigurator( &ControlConfiguration::Label );
    constexpr auto Style = MakeConfigurator( &ControlConfiguration::Style );
    constexpr auto ExStyle = MakeConfigurator( &ControlConfiguration::ExStyle );
    constexpr auto Origin = MakeConfigurator( &ControlConfiguration::Origin );
    constexpr auto Dimension = MakeConfigurator( &ControlConfiguration::Dimension );
    // constexpr ControlConfiguration MenuID( HMENU MenuID_ ) { return { .MenuID = MenuID_ }; }
    // constexpr ControlConfiguration Label( LPCSTR Label_ ) { return { .Label = Label_ }; }
    // constexpr ControlConfiguration Style( DWORD Style_ ) { return { .Style = Style_ }; }
    // constexpr ControlConfiguration ExStyle( DWORD ExStyle_ ) { return { .ExStyle = ExStyle_ }; }
    // constexpr ControlConfiguration Origin( POINT Origin_ ) { return { .Origin = Origin_ }; }
    // constexpr ControlConfiguration Dimension( SIZE Dimension_ ) { return { .Dimension = Dimension_ }; }

    struct Control : ControlConfiguration
    {
        constexpr operator HWND() const noexcept { return Handle; }

        constexpr void AddStyle( DWORD Style_ ) noexcept { *Style |= Style_; }
        constexpr void RemoveStyle( DWORD Style_ ) noexcept { *Style &= ~Style_; }
        constexpr void AddExStyle( DWORD ExStyle_ ) noexcept { *ExStyle |= ExStyle_; }
        constexpr void RemoveExStyle( DWORD ExStyle_ ) noexcept { *ExStyle &= ~ExStyle_; }

        constexpr Control()
        {
            Style = WS_VISIBLE | WS_CHILD | WS_TABSTOP;
            ExStyle = WS_EX_LEFT;
        }
    };

    struct TextLabelControl : Control
    {
        constexpr TextLabelControl() noexcept
        {
            ClassName = WC_STATIC;
            RemoveStyle( WS_TABSTOP );
        }

        void operator=( std::string_view NewText )
        {
            if( ! Handle ) return;
            Label = NewText.data();
            SetWindowText( Handle, NewText.data() );
        }
    };

    struct CanvasControl : Control
    {
        constexpr CanvasControl() noexcept
        {
            ClassName = WC_STATIC;
            RemoveStyle( WS_TABSTOP );
        }

        void Paint() const noexcept {}

        template<MatchType<CanvasContent> T>
        void Paint( T&& Content ) const noexcept
        {
            if( Handle )
            {
                CanvasContainer[Handle] = std::forward<T>( Content );
                InvalidateRect( Handle, NULL, 0 );  // trigger paint event
            }
        }
    };

    struct EditControl : Control
    {
        constexpr EditControl() noexcept { ClassName = WC_EDIT; }

        std::string Content() const
        {
            auto ResultString = std::string{};
            if( Handle )
            {
                auto RequiredBufferSize = GetWindowTextLength( Handle ) + 1;
                ResultString.resize( RequiredBufferSize, '\0' );
                GetWindowText( Handle, ResultString.data(), RequiredBufferSize );
            }
            return ResultString;
        }

        void operator=( auto&& RHS )
        {
            auto IncomingContent = std::string_view{ RHS };
            SetWindowText( Handle, IncomingContent.data() );
        }
    };

    struct TextBoxControl : EditControl
    {
        constexpr TextBoxControl() { AddStyle( ES_AUTOHSCROLL | WS_BORDER ); }
    };


    struct TextAreaControl : EditControl
    {
        constexpr TextAreaControl()
        {
            AddStyle( WS_VSCROLL | ES_AUTOVSCROLL | ES_MULTILINE | ES_WANTRETURN );
            AddExStyle( WS_EX_CONTROLPARENT );
        }
    };

    template<std::invocable ButtonEvent>
    struct ButtonControl : Control
    {
        ButtonEvent Action;

        constexpr ButtonControl( ButtonEvent Action_ ) noexcept
        {
            Action = Action_;
            ClassName = WC_BUTTON;
            AddStyle( BS_PUSHBUTTON | BS_FLAT );
        }

        void Click() const noexcept { Action(); }
    };

    constexpr auto TextLabel = TextLabelControl{};
    constexpr auto Canvas = CanvasControl{};
    constexpr auto TextBox = TextBoxControl{};
    constexpr auto TextArea = TextAreaControl{};
    constexpr auto Button = ButtonControl{ [] {} };


    struct WindowControl : BasicWindowHandle
    {
        static HWND NewWindow( LPCSTR ClassName_, LPCSTR WindowTitle_ )
        {
            HWND Handle;
            auto wc = WNDCLASSEX{};
            wc.cbSize = sizeof( WNDCLASSEX );
            wc.style = CS_HREDRAW | CS_VREDRAW;
            wc.lpfnWndProc = EWUI::WndProc;
            wc.hInstance = EWUI::EntryPointParamPack.hInstance;
            wc.hbrBackground = reinterpret_cast<HBRUSH>( COLOR_WINDOW );
            wc.lpszClassName = ClassName_;

            if( ! RegisterClassEx( &wc ) )
                return MessageBox( NULL, "Window Registration Failed!", "Error!", MB_ICONEXCLAMATION | MB_OK ), nullptr;

            Handle = CreateWindowEx( WS_EX_LEFT | WS_EX_DLGMODALFRAME,  //
                                     ClassName_, WindowTitle_,          //
                                     WS_OVERLAPPEDWINDOW,               //
                                     0, 0, 0, 0,                        //
                                     NULL, NULL, EWUI::EntryPointParamPack.hInstance, NULL );
            if( ! Handle )
                return MessageBox( NULL, "Window Creation Failed!", "Error!", MB_ICONEXCLAMATION | MB_OK ), nullptr;

            return Handle;
        }

        constexpr WindowControl() {}

        WindowControl( LPCSTR ClassName_, LPCSTR WindowName_ ) { Handle = NewWindow( ClassName_, WindowName_ ); }

        WindowControl( LPCSTR ClassName_ ) : WindowControl( ClassName_, "EWUI Window" ) {}

        WindowControl( const WindowControl& Source )
        {
            Handle = Source.Handle ? Source.Handle : NewWindow( "EWUI Window Class", "EWUI Window" );
        }

        auto operator()( LPCSTR ClassName_ ) { return WindowControl( ClassName_ ); }

        // template<DerivedFrom<ControlConfiguration> T>
        // decltype( auto ) operator<<( T&& Child_ )
        // {
        //     if( this->Handle() == NULL ) return static_cast<CRTP&>( *this );

        //     Child_.Parent( this->Handle() );
        //     if constexpr( DerivedFrom<T, WindowControl<std::decay_t<T>>> )  // pop up window
        //     {
        //         // Set Owner, not parent
        //         SetWindowLongPtr( Child_.Handle(), GWLP_HWNDPARENT, reinterpret_cast<LONG_PTR>( Child_.Parent() ) );
        //     }
        //     else
        //     {
        //         if( Child_.Origin() == POINT{ 0, 0 } ) Child_.Origin( ComponentOffset );
        //         ComponentOffset.x += Child_.Dimension().cx + ComponentSeparation.cx;
        //         ComponentTotalSize.cy = std::max( ComponentTotalSize.cy, Child_.Origin().y + Child_.Dimension().cy );
        //         ComponentTotalSize.cx = std::max( ComponentTotalSize.cx, ComponentOffset.x + Child_.Dimension().cx );
        //         Child_.Handle( CreateWindowEx( Child_.ExStyle(), Child_.ClassName(),          //
        //                                        Child_.Label(), Child_.Style(),                //
        //                                        Child_.Origin().x, Child_.Origin().y,          //
        //                                        Child_.Dimension().cx, Child_.Dimension().cy,  //
        //                                        Child_.Parent(), Child_.MenuID(), EWUI::EntryPointParamPack.hInstance,
        //                                        NULL ) );

        //         if constexpr( MatchType<T, Button> ) ActionContainer.RegisterAction( Child_.Handle(), Child_.Action() );

        //         if constexpr( MatchType<T, Canvas> )
        //             PainterContainer.RegisterAction( Child_.Handle(), Child_.Painter() );
        //     }

        //     return static_cast<CRTP&>( *this );
        // }

        // decltype( auto ) operator<<( std::string_view TextContent )
        // {
        //     SIZE TextContentDimension;
        //     auto hdc = GetDC( this->Handle() );
        //     GetTextExtentPoint32( hdc, TextContent.data(), TextContent.length(), &TextContentDimension );
        //     ReleaseDC( this->Handle(), hdc );
        //     return this->operator<<( TextLabel().Label( TextContent.data() ).Dimension( TextContentDimension ) );
        // }

        // decltype( auto ) operator<<( LineBreaker )
        // {
        //     ComponentOffset.y = ComponentTotalSize.cy + ComponentSeparation.cy;
        //     ComponentOffset.x = ComponentSeparation.cx;
        //     return static_cast<CRTP&>( *this );
        // }

        // decltype( auto ) operator<<( Spacer Spacer_ )
        // {
        //     ComponentOffset.x += Spacer_.x;
        //     ComponentTotalSize.cx = std::max( ComponentTotalSize.cx, ComponentOffset.x );
        //     return static_cast<CRTP&>( *this );
        // }


        void SetWindowVisibility( int nCmdShow ) noexcept
        {
            if( Handle ) ShowWindow( Handle, nCmdShow );
        }

        void Show() noexcept { SetWindowVisibility( SW_SHOW ); }
        void Hide() noexcept { SetWindowVisibility( SW_HIDE ); }

        void ToggleVisibility() noexcept
        {
            if( ! Handle ) return;
            if( GetWindowLong( Handle, GWL_STYLE ) & WS_VISIBLE )
                Hide();
            else
                Show();
        }

        int Activate() const
        {
            if( ! Handle ) return 0;

            ShowWindow( Handle, EWUI::EntryPointParamPack.nCmdShow );
            UpdateWindow( Handle );

            MSG Msg{};
            while( GetMessage( &Msg, NULL, 0, 0 ) > 0 )
            {
                if( ( Msg.message == WM_KEYDOWN || Msg.message == WM_KEYUP )  //
                    && Msg.wParam == VK_RETURN                                //
                    && MatchClass<WC_EDIT>( Msg.hwnd )                        //
                    && ( GetWindowLong( Msg.hwnd, GWL_STYLE ) & ES_WANTRETURN ) == 0 )
                {
                    Msg.wParam = VK_TAB;
                }

                if( ! IsDialogMessage( Handle, &Msg ) )
                {
                    //PrintMSG( ">>", Msg );
                    TranslateMessage( &Msg );
                    DispatchMessage( &Msg );
                }
            }
            return static_cast<int>( Msg.wParam );
        }

        operator int() const { return Activate(); }
    };

    struct PopupWindowControl : WindowControl
    {
        constexpr PopupWindowControl() {}
        PopupWindowControl( LPCSTR ClassName_ ) : WindowControl( ClassName_, "EWUI Popup Window" ) {}
        PopupWindowControl( const PopupWindowControl& Source )
        {
            Handle = Source.Handle ? Source.Handle : NewWindow( "EWUI Popup Window Class", "EWUI Popup Window" );
        }
    };

    inline auto Window = WindowControl{};

    inline auto PopupWindow = PopupWindowControl{};

    template<DerivedFrom<ControlConfiguration> T>
    constexpr decltype( auto ) operator<<( T LHS, T&& RHS )
    {
        constexpr auto OptionalCopy = [&]( auto... Fs ) { ( (void)( RHS.*Fs && ( LHS.*Fs = RHS.*Fs ) ), ... ); };
        using P = ControlConfiguration;
        OptionalCopy( &P::ClassName, &P::Label, &P::MenuID, &P::Style, &P::ExStyle, &P::Origin, &P::Dimension );
        return LHS;
    }

    template<DerivedFrom<ControlConfiguration> T>
    constexpr decltype( auto ) operator<<( T LHS, LPCSTR RHS )
    {
        LHS.Label = RHS;
        return LHS;
    }


    [[nodiscard( "" )]] decltype( auto ) operator<<( DerivedFrom<WindowControl> auto& LHS,
                                                     const ControlConfiguration&      RHS )
    {
        if( LHS.Handle )
        {
            return std::move( LHS ) << RHS;
        }
        else
        {
            auto NewWindowControl = LHS;
            return std::move( NewWindowControl ) << RHS;
        }
    }

    decltype( auto ) operator<<( DerivedFrom<WindowControl> auto&& LHS, const ControlConfiguration& RHS )
    {
        if( LHS.Handle )
        {
            if( RHS.Label ) SetWindowText( LHS.Handle, *RHS.Label );
            if( RHS.Style ) LHS.AddStyle( *RHS.Style );
            if( RHS.ExStyle ) LHS.AddExStyle( *RHS.ExStyle );
            if( RHS.Dimension ) LHS.ReSize( *RHS.Dimension );
            if( RHS.Origin ) LHS.MoveTo( *RHS.Origin );
        }
        return std::forward<decltype( LHS )>( LHS );
    }

    decltype( auto ) operator|( DerivedFrom<WindowControl> auto&& LHS, DerivedFrom<Control> auto&& RHS )
    {
        return std::forward<decltype( LHS )>( LHS );
    }

    decltype( auto ) operator|( DerivedFrom<WindowControl> auto&& LHS, LPCSTR RHS )
    {
        return std::forward<decltype( LHS )>( LHS ) | TextLabel << RHS;
    }

    decltype( auto ) operator|( DerivedFrom<WindowControl> auto&& LHS, LineBreaker )
    {
        return std::forward<decltype( LHS )>( LHS );
    }


}  // namespace EWUI

int WINAPI WinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow )
{
    EWUI::EntryPointParamPack = { hInstance, hPrevInstance, lpCmdLine, nCmdShow };
    return EWUI::Main();
}
#endif
