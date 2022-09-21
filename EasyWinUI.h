#ifndef EASYWINUI_H
#define EASYWINUI_H

#include "EasyWinUI.h"
#include <Windows.h>
#include <CommCtrl.h>
#include <gdiplus.h>

#include <algorithm>
#include <concepts>
#include <cstring>
#include <functional>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <vector>


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

    void PrintMSG( std::string_view PrefixString, MSG Msg )
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
}  // namespace

namespace EWUI
{
    int Main();

    struct Spacer
    {
        int x{};
    };

    struct LineBreaker
    {
    } LineBreak;

    constexpr auto EmptyAction = [] {};
    using ControlAction = std::function<void()>;
    std::map<HWND, ControlAction> ActionContainer;

    using ByteVector = std::vector<std::byte>;
    using CanvasContent = std::pair<BITMAPINFO, ByteVector>;
    std::map<HWND, CanvasContent> CanvasContainer;

    LRESULT CALLBACK WndProc( HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam )
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

    struct EntryPointParamPack_
    {
        HINSTANCE hInstance;
        HINSTANCE hPrevInstance;
        LPCSTR    lpCmdLine;
        int       nCmdShow;
    } EntryPointParamPack{};

    struct ControlConfiguration
    {
        std::optional<HWND>   Handle{};
        std::optional<HWND>   Parent{};
        std::optional<HMENU>  MenuID{};
        std::optional<LPCSTR> ClassName{};
        std::optional<LPCSTR> Label{};
        std::optional<DWORD>  Style{ WS_VISIBLE | WS_CHILD | WS_TABSTOP };
        std::optional<DWORD>  ExStyle{};
        std::optional<POINT>  Origin{};
        std::optional<SIZE>   Dimension{};
    };

    constexpr ControlConfiguration MenuID( HMENU MenuID_ ) { return { .MenuID = MenuID_ }; }
    constexpr ControlConfiguration Label( LPCSTR Label_ ) { return { .Label = Label_ }; }
    constexpr ControlConfiguration Style( DWORD Style_ ) { return { .Style = Style_ }; }
    constexpr ControlConfiguration ExStyle( DWORD ExStyle_ ) { return { .ExStyle = ExStyle_ }; }
    constexpr ControlConfiguration Origin( POINT Origin_ ) { return { .Origin = Origin_ }; }
    constexpr ControlConfiguration Dimension( SIZE Dimension_ ) { return { .Dimension = Dimension_ }; }

    // Window{ .ClassName = "EWUI Window Class", .Label = "EWUI Window" },  //
    // PopupWindow{.ClassName = "EWUI Popup Window Class",.Label = "EWUI Popup Window"},//
    // TextBox;

    struct Control : ControlConfiguration
    {
        constexpr operator HWND() const noexcept { return *Handle; }

        constexpr void AddStyle( DWORD Style_ ) noexcept { *Style |= Style_; }
        constexpr void RemoveStyle( DWORD Style_ ) noexcept { *Style &= ~Style_; }
        constexpr void AddExStyle( DWORD ExStyle_ ) noexcept { *ExStyle |= ExStyle_; }
        constexpr void RemoveExStyle( DWORD ExStyle_ ) noexcept { *ExStyle &= ~ExStyle_; }

        auto GetRect() const noexcept
        {
            RECT rect;
            GetWindowRect( *Handle, &rect );
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

        auto ReSize( SIZE NewDimension ) const noexcept
        {
            SetWindowPos( *Handle, HWND_NOTOPMOST, 0, 0, NewDimension.cx, NewDimension.cy,
                          SWP_SHOWWINDOW | SWP_NOMOVE | SWP_NOZORDER );
        }

        auto MoveTo( POINT NewPoint ) const noexcept
        {
            SetWindowPos( *Handle, HWND_NOTOPMOST, NewPoint.x, NewPoint.y, 0, 0,
                          SWP_SHOWWINDOW | SWP_NOSIZE | SWP_NOZORDER );
        }
    };

    struct TextLabelControl : Control
    {
        // TextLabel()
        // {
        //     this->ClassName( WC_STATIC );
        //     this->RemoveStyle( WS_TABSTOP );
        // }

        void operator=( std::string_view NewText )
        {
            if( *Handle == NULL ) return;
            Label = NewText.data();
            SetWindowText( *Handle, NewText.data() );
        }
    };

    constexpr auto TextLabel = [] {
        auto TextLabel = TextLabelControl{};
        TextLabel.ClassName = WC_STATIC;
        TextLabel.RemoveStyle( WS_TABSTOP );
        return TextLabel;
    }();

    struct CanvasControl : Control
    {
        // Canvas()
        // {
        //     this->ClassName( WC_STATIC );
        //     this->RemoveStyle( WS_TABSTOP );
        // }

        void Paint() const noexcept {}

        template<MatchType<CanvasContent> T>
        void Paint( T&& Content ) const noexcept
        {
            if( Handle && *Handle )
            {
                CanvasContainer[*Handle] = std::forward<T>( Content );
                InvalidateRect( *Handle, NULL, 0 );  // trigger paint event
            }
        }
    };

    constexpr auto Canvas = [] {
        auto Canvas = CanvasControl{};
        Canvas.ClassName = WC_STATIC;
        Canvas.RemoveStyle( WS_TABSTOP );
        return Canvas;
    }();

    template<std::invocable ButtonEvent>
    struct ButtonControl : Control
    {
        ButtonEvent Action;

        // constexpr ButtonControl(ButtonEvent Action_)
        // {
        //     Action = Action_;
        //     // this->ClassName( WC_BUTTON );
        //     // this->AddStyle( BS_PUSHBUTTON | BS_FLAT );
        // }

        void Click() const noexcept { Action(); }
    };

    constexpr auto Button = [] {
        auto Button = ButtonControl<decltype( [] {} )>{};
        Button.ClassName = WC_BUTTON;
        Button.AddStyle( BS_PUSHBUTTON | BS_FLAT );
        return Button;
    }();

    struct EditControl : Control
    {
        EditControl() { *ClassName = WC_EDIT; }

        std::string Content()
        {
            if( ! Handle || *Handle == NULL ) return {};
            auto RequiredBufferSize = GetWindowTextLength( *Handle ) + 1;
            auto ResultString = std::string( RequiredBufferSize, '\0' );
            GetWindowText( *Handle, ResultString.data(), RequiredBufferSize );
            return ResultString;
        }

        void operator=( auto&& RHS )
        {
            auto IncomingContent = std::string_view{ RHS };
            SetWindowText( *Handle, IncomingContent.data() );
        }
    };

    struct TextBox : EditControl
    {
        using EditControl::operator=;
        TextBox() { AddStyle( ES_AUTOHSCROLL | WS_BORDER ); }
    };

    struct TextArea : EditControl
    {
        using EditControl::operator=;
        TextArea()
        {
            AddStyle( WS_VSCROLL | ES_AUTOVSCROLL | ES_MULTILINE | ES_WANTRETURN );
            AddExStyle( WS_EX_CONTROLPARENT );
        }
    };

    struct WindowControl : Control
    {
        constexpr static SIZE ComponentSeparation{ 10, 10 };
        POINT                 ComponentOffset{ 0, 0 };
        SIZE                  ComponentTotalSize{ 0, 0 };
        // PAINTSTRUCT ps;
        // HDC hdc;

        WindowControl( LPCSTR ClassName_, LPCSTR Label_ )
        {
            ClassName = ClassName_;
            Label = Label_;

            RemoveStyle( WS_CHILD | WS_VISIBLE );

            AddStyle( WS_OVERLAPPEDWINDOW );
            AddExStyle( WS_EX_LEFT | WS_EX_DLGMODALFRAME );  // | WS_EX_LAYERED

            auto wc = WNDCLASSEX{};
            wc.cbSize = sizeof( WNDCLASSEX );
            wc.style = CS_HREDRAW | CS_VREDRAW;
            wc.lpfnWndProc = EWUI::WndProc;
            wc.hInstance = EWUI::EntryPointParamPack.hInstance;
            wc.hbrBackground = reinterpret_cast<HBRUSH>( COLOR_WINDOW );
            wc.lpszClassName = *ClassName;

            if( ! RegisterClassEx( &wc ) )
            {
                MessageBox( NULL, "Window Registration Failed!", "Error!", MB_ICONEXCLAMATION | MB_OK );
                return;
            }

            Handle = CreateWindowEx( *ExStyle, *ClassName, *Label, *Style,                //
                                     Origin->x, Origin->y, Dimension->cx, Dimension->cy,  //
                                     *Parent, *MenuID, EWUI::EntryPointParamPack.hInstance, NULL );
            if( *Handle == NULL )
            {
                MessageBox( NULL, "Window Creation Failed!", "Error!", MB_ICONEXCLAMATION | MB_OK );
                return;
            }

            Show();
        }

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
            if( Handle && *Handle ) ShowWindow( *Handle, nCmdShow );
        }

        void Show() noexcept { SetWindowVisibility( SW_SHOW ); }
        void Hide() noexcept { SetWindowVisibility( SW_HIDE ); }

        void ToggleVisibility() noexcept
        {
            if( ! Handle ) return;
            if( GetWindowLong( *Handle, GWL_STYLE ) & WS_VISIBLE )
                Hide();
            else
                Show();
        }
    };

    struct Window : WindowControl
    {
        Window( LPCSTR ClassName_ = {}, LPCSTR Label_ = {} )
            : WindowControl( ( ClassName_ == NULL || strlen( ClassName_ ) == 0 ) ? "EWUI Window Class" : ClassName_,
                             ( Label_ == NULL || strlen( Label_ ) == 0 ) ? "EWUI Window Title" : Label_ )
        {}

        int Activate() const
        {
            if( ! Handle || *Handle == NULL ) return 0;

            if( ! Dimension )
            {
                // buffer space for title bar
                ReSize( { ComponentTotalSize.cx, ComponentTotalSize.cy + 40 } );
            }

            ShowWindow( *Handle, EWUI::EntryPointParamPack.nCmdShow );
            UpdateWindow( *Handle );

            MSG        Msg;
            static int msg_counter = 0;
            while( GetMessage( &Msg, NULL, 0, 0 ) > 0 )
            {
                if( ( Msg.message == WM_KEYDOWN || Msg.message == WM_KEYUP )  //
                    && Msg.wParam == VK_RETURN                                //
                    && MatchClass<WC_EDIT>( Msg.hwnd )                        //
                    && ( GetWindowLong( Msg.hwnd, GWL_STYLE ) & ES_WANTRETURN ) == 0 )
                {
                    Msg.wParam = VK_TAB;
                }

                if( ! IsDialogMessage( this->Handle(), &Msg ) )
                {
                    PrintMSG( ">>", Msg );

                    TranslateMessage( &Msg );
                    DispatchMessage( &Msg );
                }
            }
            return static_cast<int>( Msg.wParam );
        }

        operator int() const { return this->Activate(); }
    };

    struct PopupWindow : WindowControl
    {
        PopupWindow( LPCSTR ClassName_ = {}, LPCSTR Label_ = {} )
            : WindowControl(
                  ( ClassName_ == NULL || strlen( ClassName_ ) == 0 ) ? "EWUI Popup Window Class" : ClassName_,
                  ( Label_ == NULL || strlen( Label_ ) == 0 ) ? "EWUI Popup Window Title" : Label_ )
        {}
    };


    //#define OptionalCopy( Field ) if( RHS.Field ) LHS.Field = RHS.Field
    template<typename T = ControlConfiguration>
    constexpr auto OptionalCopy( DerivedFrom<T> auto& LHS, DerivedFrom<T> auto&& RHS, auto T::*Field )
    {
        if( RHS.*Field ) LHS.*Field = RHS.*Field;
    }

    template<typename T = ControlConfiguration>
    constexpr auto operator<<( DerivedFrom<T> auto LHS, DerivedFrom<T> auto&& RHS )
    {
        OptionalCopy( LHS, RHS, &T::ClassName );
        OptionalCopy( LHS, RHS, &T::Label );
        OptionalCopy( LHS, RHS, &T::MenuID );
        OptionalCopy( LHS, RHS, &T::Style );
        OptionalCopy( LHS, RHS, &T::ExStyle );
        OptionalCopy( LHS, RHS, &T::Origin );
        OptionalCopy( LHS, RHS, &T::Dimension );

        return LHS;
    }

    constexpr auto operator<<( DerivedFrom<ControlConfiguration> auto LHS, LPCSTR RHS )
    {
        LHS.Label = RHS;

        return LHS;
    }

}  // namespace EWUI

int WINAPI WinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow )
{
    EWUI::EntryPointParamPack = { hInstance, hPrevInstance, lpCmdLine, nCmdShow };
    return EWUI::Main();
}
#endif
