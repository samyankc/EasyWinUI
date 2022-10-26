#ifndef EASYWINUI_H
#define EASYWINUI_H

#include <Windows.h>
#include <CommCtrl.h>
#include <cstddef>

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
#include <type_traits>
#include <vector>

#define EVENT_PARAMETER_LIST HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam

// helpers
namespace
{
    template<typename Derived, typename Base>
    concept DerivedFrom = std::is_base_of_v<std::decay_t<Base>, std::decay_t<Derived>>;

    template<typename TestClass, template<typename> typename TemplateClass>
    struct SpecializationOf_impl : std::false_type
    {};

    template<template<typename> typename TemplateClass, typename T>
    struct SpecializationOf_impl<TemplateClass<T>, TemplateClass> : std::true_type
    {};

    template<typename TestClass, template<typename> typename TemplateClass>
    concept SpecializationOf = SpecializationOf_impl<TestClass, TemplateClass>::value;

    template<typename TargetType, typename... CandidateTypes>
    concept MatchExactType = ( std::same_as<TargetType, CandidateTypes> || ... );

    template<typename TargetType, typename... CandidateTypes>
    concept MatchType = MatchExactType<std::decay_t<TargetType>, std::decay_t<CandidateTypes>...>;

    // template<MatchType<SIZE, POINT> T>
    // bool operator==( const T& LHS, const T& RHS )
    // {
    //     return std::memcmp( &LHS, &RHS, sizeof( T ) ) == 0;
    // }

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
    } LineBreak, NewLine;

    constexpr auto AlwaysZero = 0L;

    constexpr auto EmptyAction = [] {};
    using ControlAction = std::function<void()>;
    inline std::map<HWND, ControlAction> ActionContainer;

    using ByteVector = std::vector<BYTE>;
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
                if( ! ActionContainer.contains( std::bit_cast<HWND>( lParam ) ) )
                    return DefWindowProc( hwnd, msg, wParam, lParam );
                std::thread( ActionContainer[std::bit_cast<HWND>( lParam )] ).detach();
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

        auto GetStyle() const noexcept { return GetWindowLong( Handle, GWL_STYLE ); }
        auto GetExStyle() const noexcept { return GetWindowLong( Handle, GWL_EXSTYLE ); }

        auto ReSize( SIZE NewDimension ) const noexcept
        {
            SetWindowPos( Handle, HWND_NOTOPMOST, 0, 0, NewDimension.cx, NewDimension.cy,
                          // SWP_SHOWWINDOW |
                          SWP_NOMOVE | SWP_NOZORDER );
        }

        auto MoveTo( POINT NewPoint ) const noexcept
        {
            SetWindowPos( Handle, HWND_NOTOPMOST, NewPoint.x, NewPoint.y, 0, 0,
                          SWP_SHOWWINDOW | SWP_NOSIZE | SWP_NOZORDER );
        }

        auto AddStyle( DWORD NewStyle ) const noexcept
        {
            return SetWindowLong( Handle, GWL_STYLE, GetStyle() | NewStyle );
        }

        auto RemoveStyle( DWORD TargetStyle ) const noexcept
        {
            return SetWindowLong( Handle, GWL_STYLE, GetStyle() & ~TargetStyle );
        }

        auto AddExStyle( DWORD NewExStyle ) const noexcept
        {
            return SetWindowLong( Handle, GWL_EXSTYLE, GetExStyle() | NewExStyle );
        }

        auto RemoveExStyle( DWORD TargetExStyle ) const noexcept
        {
            return SetWindowLong( Handle, GWL_EXSTYLE, GetExStyle() & ~TargetExStyle );
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

        constexpr Control() noexcept
        {
            Style = WS_VISIBLE | WS_CHILD | WS_TABSTOP;
            ExStyle = WS_EX_LEFT;
        }

        auto SetLabel( std::string_view IncomingContent ) const noexcept
        {
            SetWindowText( Handle, IncomingContent.data() );
        }

        auto Content() const noexcept
        {
            auto ResultString = std::string{};
            if( Handle )
            {
                ResultString.resize( GetWindowTextLength( Handle ), '\0' );
                GetWindowText( Handle, ResultString.data(), ResultString.length() + 1 );
            }
            return ResultString;
        }

        auto operator=( std::string_view IncomingContent ) const noexcept { return SetLabel( IncomingContent ); }
    };

    struct TextLabelControl : Control
    {
        constexpr TextLabelControl() noexcept
        {
            ClassName = WC_STATIC;
            RemoveStyle( WS_TABSTOP );
        }

        using Control::operator=;
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

        using Control::operator=;
    };

    struct TextBoxControl : EditControl
    {
        constexpr TextBoxControl() noexcept { AddStyle( ES_AUTOHSCROLL | WS_BORDER ); }
    };

    struct TextAreaControl : EditControl
    {
        constexpr TextAreaControl() noexcept
        {
            AddStyle( WS_VSCROLL | ES_AUTOVSCROLL | ES_MULTILINE | ES_WANTRETURN );
            AddExStyle( WS_EX_CONTROLPARENT );
        }
    };

    struct ListViewControl : Control
    {
        constexpr ListViewControl() noexcept
        {
            ClassName = WC_LISTVIEW;
            AddStyle( LVS_LIST | LVS_SINGLESEL );
        }
    };

    struct ListBoxControl : Control
    {
        using DataContainer = std::vector<
            std::pair<std::invoke_result_t<decltype( SendMessage ), HWND, UINT, WPARAM, LPARAM>, std::string>>;

        constexpr ListBoxControl() noexcept
        {
            ClassName = WC_LISTBOX;
            AddStyle( WS_VSCROLL | LBS_USETABSTOPS | LBS_WANTKEYBOARDINPUT | LBS_HASSTRINGS );
        }

        void operator<<( const DataContainer& Source ) const noexcept
        {
            if( ! Handle ) return;
            for( auto&& [ItemData, DisplayString] : Source )
            {
                auto ListIndex =
                    SendMessage( Handle, LB_ADDSTRING, AlwaysZero, std::bit_cast<LPARAM>( DisplayString.c_str() ) );
                SendMessage( Handle, LB_SETITEMDATA, ListIndex, std::bit_cast<LPARAM>( ItemData ) );
            }
        };

        auto Selection() const noexcept
        {
            auto SelectedIndex = SendMessage( Handle, LB_GETCURSEL, AlwaysZero, AlwaysZero );
            return SendMessage( Handle, LB_GETITEMDATA, SelectedIndex, AlwaysZero );
        }

        auto Reset() const noexcept { SendMessage( Handle, LB_RESETCONTENT, AlwaysZero, AlwaysZero ); }
    };


    struct ProgressBarControl : Control
    {
        constexpr ProgressBarControl() noexcept
        {
            ClassName = PROGRESS_CLASS;
            AddStyle( PBS_SMOOTH | PBS_SMOOTHREVERSE | PBS_MARQUEE );
            RemoveStyle( WS_TABSTOP );
        }

        auto Pause() const noexcept { SendMessage( Handle, PBM_SETSTATE, PBST_PAUSED, AlwaysZero ); }
        auto Resume() const noexcept { SendMessage( Handle, PBM_SETSTATE, PBST_NORMAL, AlwaysZero ); }
        auto Reset() const noexcept { SendMessage( Handle, PBM_SETPOS, 0, AlwaysZero ); }
        auto SetMax( LPARAM NewMax = 100 ) const noexcept { SendMessage( Handle, PBM_SETRANGE32, 0, NewMax ); }
        auto Advance( WPARAM Delta = 10 ) const noexcept { SendMessage( Handle, PBM_DELTAPOS, Delta, AlwaysZero ); }
    };

    template<std::invocable ButtonEvent>
    struct ButtonControl : Control
    {
        ButtonEvent Action;

        constexpr ButtonControl( ButtonEvent Action_ ) noexcept : Action{ Action_ }
        {
            ClassName = WC_BUTTON;
            AddStyle( BS_PUSHBUTTON | BS_FLAT );
        }

        void Click() const noexcept { Action(); }

        template<std::invocable NewButtonEvent>
        constexpr auto operator<<( NewButtonEvent Action_ ) const noexcept
        {
            auto NewButton = ButtonControl<NewButtonEvent>{ Action_ };
            NewButton << *this;
            return NewButton;
        }
    };

    constexpr auto TextLabel = TextLabelControl{};
    constexpr auto Canvas = CanvasControl{};
    constexpr auto TextBox = TextBoxControl{};
    constexpr auto TextArea = TextAreaControl{};
    constexpr auto ListBox = ListBoxControl{};
    constexpr auto ListView = ListViewControl{};
    constexpr auto ProgressBar = ProgressBarControl{};
    constexpr auto Button = ButtonControl{ [] {} };

    struct WindowControl : BasicWindowHandle
    {
        constexpr static auto ChildSeparation = 20;

        SIZE  RequiredDimension{};
        POINT AnchorOffset{ ChildSeparation, ChildSeparation };

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

            if( ! RegisterClassEx( &wc ) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS )
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

        auto operator()( LPCSTR ClassName_ ) const { return WindowControl( ClassName_ ); }

        void SetWindowVisibility( int nCmdShow ) const noexcept
        {
            if( Handle ) ShowWindow( Handle, nCmdShow );
        }

        void Show() const noexcept { SetWindowVisibility( SW_SHOW ); }
        void Hide() const noexcept { SetWindowVisibility( SW_HIDE ); }

        void ToggleVisibility() const noexcept
        {
            if( ! Handle ) return;
            if( GetWindowLong( Handle, GWL_STYLE ) & WS_VISIBLE )
                Hide();
            else
                Show();
        }

        int Activate() const noexcept
        {
            if( ! Handle ) return 0;

            ReSize( { RequiredDimension.cx + WindowControl::ChildSeparation,
                      RequiredDimension.cy + 2 * WindowControl::ChildSeparation } );

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

        operator int() const noexcept { return Activate(); }
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

    inline constexpr auto Window = WindowControl{};

    inline constexpr auto PopupWindow = PopupWindowControl{};

    template<DerivedFrom<ControlConfiguration> T>
    //constexpr decltype( auto ) operator<<( T&& LHS, const ControlConfiguration& RHS )
    constexpr decltype( auto ) operator<<( T&& LHS, const ControlConfiguration& RHS )
    {
        if constexpr( std::is_const_v<std::remove_reference_t<T>> )
        {
            return std::decay_t<T>( std::decay_t<T>( LHS ) << RHS );
        }
        else
        {
            using P = ControlConfiguration;
            auto OptionalCopy = [&]( auto... Fs ) { ( (void)( RHS.*Fs && ( LHS.*Fs = RHS.*Fs ) ), ... ); };
            OptionalCopy( &P::ClassName, &P::Label, &P::MenuID, &P::Style, &P::ExStyle, &P::Origin, &P::Dimension );
            return std::forward<T>( LHS );
        }
    }

    template<DerivedFrom<BasicWindowHandle> T>
    //constexpr decltype( auto ) operator<<( T&& LHS, LPCSTR RHS )
    constexpr decltype( auto ) operator<<( T&& LHS, LPCSTR RHS )
    {
        return std::forward<T>( LHS ) << Label( RHS );
    }

    template<DerivedFrom<WindowControl> T>
    // decltype( auto ) operator<<( T&& LHS, const ControlConfiguration& RHS )
    decltype( auto ) operator<<( T&& LHS, const ControlConfiguration& RHS )
    {
        if constexpr( std::is_const_v<std::remove_reference_t<T>> )
        {
            return std::decay_t<T>( std::decay_t<T>( LHS ) << RHS );
        }
        else
        {
            if( ! LHS.Handle ) return std::forward<T>( LHS );

            if( RHS.Label ) SetWindowText( LHS.Handle, *RHS.Label );
            if( RHS.Style ) LHS.AddStyle( *RHS.Style );
            if( RHS.ExStyle ) LHS.AddExStyle( *RHS.ExStyle );
            if( RHS.Dimension ) LHS.ReSize( *RHS.Dimension );
            if( RHS.Origin ) LHS.MoveTo( *RHS.Origin );

            return std::forward<T>( LHS );
        }
    }

    template<DerivedFrom<WindowControl> T>
    decltype( auto ) operator|( T&& LHS, DerivedFrom<Control> auto&& RHS )
    {
        if( ! LHS.Handle ) return std::forward<T>( LHS );
        constexpr auto Read = []<typename U>( const std::optional<U>& Field ) { return Field.value_or( U{} ); };

        auto ChildDimension = RHS.Dimension.value_or( SIZE{ 200, 20 } );
        auto ChildOrigin = RHS.Origin.value_or( LHS.AnchorOffset );

        LHS.RequiredDimension = { std::max( LHS.AnchorOffset.x += ChildDimension.cx + WindowControl::ChildSeparation,
                                            LHS.RequiredDimension.cx ),
                                  std::max( LHS.AnchorOffset.y + ChildDimension.cy + WindowControl::ChildSeparation,
                                            LHS.RequiredDimension.cy ) };

        auto Handle_ = CreateWindowEx( Read( RHS.ExStyle ), Read( RHS.ClassName ),  //
                                       Read( RHS.Label ), Read( RHS.Style ),        //
                                       ChildOrigin.x, ChildOrigin.y,                //
                                       ChildDimension.cx, ChildDimension.cy,        //
                                       LHS.Handle, Read( RHS.MenuID ), EWUI::EntryPointParamPack.hInstance, NULL );

        using ChildType = std::remove_reference_t<decltype( RHS )>;
        if constexpr( ! std::is_const_v<ChildType> ) RHS.Handle = Handle_;
        if constexpr( SpecializationOf<ChildType, ButtonControl> ) ActionContainer[Handle_] = RHS.Action;

        return std::forward<T>( LHS );
    }

    template<DerivedFrom<WindowControl> T>
    decltype( auto ) operator|( T&& LHS, DerivedFrom<WindowControl> auto&& RHS )
    {
        SetWindowLongPtr( RHS.Handle, GWLP_HWNDPARENT, std::bit_cast<LONG_PTR>( LHS.Handle ) );
        return std::forward<T>( LHS );
    }

    template<DerivedFrom<WindowControl> T>
    decltype( auto ) operator|( T&& LHS, LPCSTR RHS )
    {
        return std::forward<T>( LHS ) | TextLabel << Dimension( { 40, 20 } ) << Label( RHS );
    }

    template<DerivedFrom<WindowControl> T>
    decltype( auto ) operator|( T&& LHS, LineBreaker )
    {
        LHS.AnchorOffset = { WindowControl::ChildSeparation, LHS.RequiredDimension.cy };
        return std::forward<T>( LHS );
    }


}  // namespace EWUI

int WINAPI WinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow )
{
    EWUI::EntryPointParamPack = { hInstance, hPrevInstance, lpCmdLine, nCmdShow };
    return EWUI::Main();
}
#endif
