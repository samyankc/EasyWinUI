#ifndef EASYWINUI_H
#define EASYWINUI_H

#include <windows.h>
#include <CommCtrl.h>

#include <algorithm>
#include <concepts>
#include <cstddef>
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
#include <type_traits>
#include <vector>
#include <winnt.h>

#define EVENT_PARAMETER_LIST HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam

// helpers
namespace
{

    constexpr inline auto MaxClassNameLength = 256uz;

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

    template<std::size_t N>
    struct FixedString
    {
        char data[N]{};

        constexpr FixedString( const char ( &Src )[N] ) { std::ranges::copy( Src, data ); }
        constexpr auto BufferSize() const noexcept { return N; }

        // constexpr auto ToStringView() const noexcept { return std::string_view{ data }; }
        // constexpr auto length() const noexcept { return ToStringView().length(); }

        constexpr operator std::string_view() const noexcept { return { data }; }
    };

    template<std::size_t N>
    FixedString( const char ( & )[N] ) -> FixedString<N>;

    template<FixedString TargetClassName>
    bool MatchClass( HWND Handle )
    {
        constexpr auto BufferSize = TargetClassName.BufferSize();
        char           Buffer[BufferSize];
        GetClassName( Handle, Buffer, BufferSize );
        return std::string_view{ Buffer } == TargetClassName;
    }

    inline auto NameOfHandle( HWND Handle )
    {
        std::string ResultString;
        ResultString.resize_and_overwrite( GetWindowTextLength( Handle ),
                                           [Handle = Handle]( auto Buffer, auto BufferSize ) {
                                               return GetWindowText( Handle, Buffer, BufferSize + 1 );
                                           } );
        return ResultString;
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

    constexpr auto AlwaysZero = 0LL;

    constexpr auto EmptyAction = [] {};
    using ControlAction = std::function<void()>;
    inline std::map<HWND, ControlAction> ActionContainer;

    //using ByteVector = std::vector<BYTE>;
    using CanvasContent = std::pair<BITMAPINFO, std::vector<RGBQUAD>>;
    inline std::map<HWND, CanvasContent> CanvasContainer;

    inline LRESULT CALLBACK WndProc( HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam )
    {
        switch( msg )
        {
            // case WM_CREATE : break;
            // case WM_MOVE :
            // case WM_SIZE : InvalidateRect( hwnd, NULL, true ); break;
            // case WM_ERASEBKGND : return true;
            case WM_PAINT :
            {
                PAINTSTRUCT PS;
                //auto        hdc = //
                BeginPaint( hwnd, &PS );
                for( auto&& [CanvasHandle, CanvasContent] : CanvasContainer )
                {
                    if( GetParent( CanvasHandle ) == hwnd )
                    {
                        auto&& [BI, Pixels] = CanvasContent;
                        auto hdc = GetDC( CanvasHandle );
                        SetDIBitsToDevice( hdc, 0, 0, std::abs( BI.bmiHeader.biWidth ),
                                           std::abs( BI.bmiHeader.biHeight ), 0, 0, 0,
                                           std::abs( BI.bmiHeader.biHeight ), Pixels.data(), &BI, DIB_RGB_COLORS );
                        ReleaseDC( CanvasHandle, hdc );
                    }
                }
                EndPaint( hwnd, &PS );
                break;
            }
            case WM_COMMAND :
            {
                if( ActionContainer.contains( std::bit_cast<HWND>( lParam ) ) )
                    std::thread( ActionContainer[std::bit_cast<HWND>( lParam )] ).detach();
                break;
            }
            case WM_CLOSE :
            {
                if( GetWindow( hwnd, GW_OWNER ) == NULL )
                    DestroyWindow( hwnd );
                else
                    ShowWindow( hwnd, SW_HIDE );
                return 0;
            }
            case WM_DESTROY :
            {
                PostQuitMessage( 0 );
                return 0;
            }
            default : break;
        }

        return DefWindowProc( hwnd, msg, wParam, lParam );
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

        template<auto RectRetriever>
        auto GetRect() const noexcept
        {
            RECT rect;
            RectRetriever( Handle, &rect );
            return rect;
        }

        auto Width() const noexcept
        {
            auto rect = GetRect<GetWindowRect>();
            return rect.right - rect.left;
        }

        auto Height() const noexcept
        {
            auto rect = GetRect<GetWindowRect>();
            return rect.bottom - rect.top;
        }

        auto GetStyle() const noexcept { return GetWindowLong( Handle, GWL_STYLE ); }
        auto GetExStyle() const noexcept { return GetWindowLong( Handle, GWL_EXSTYLE ); }

        auto ReSize( SIZE NewDimension ) const noexcept
        {
            auto ClientRect = RECT{ 0, 0, NewDimension.cx, NewDimension.cy };
            AdjustWindowRect( &ClientRect, GetStyle(), false );
            auto NewWidth{ ClientRect.right - ClientRect.left },  //
                NewHeight{ ClientRect.bottom - ClientRect.top };
            SetWindowPos( Handle, HWND_NOTOPMOST, 0, 0, NewWidth, NewHeight,
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

        auto TextContent() const noexcept
        {
            auto ResultString = std::string{};
            if( Handle )
                ResultString.resize_and_overwrite( GetWindowTextLength( Handle ),
                                                   [Handle = Handle]( auto Buffer, auto BufferSize ) {
                                                       return GetWindowText( Handle, Buffer, BufferSize + 1 );
                                                   } );
            return ResultString;
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
            auto Result = ControlConfiguration{};
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

        auto SetContent( std::string_view IncomingContent ) const noexcept
        {
            SetWindowText( Handle, IncomingContent.data() );
        }

        auto SetLabel( std::string_view IncomingContent ) const noexcept
        {
            SetWindowText( Handle, IncomingContent.data() );
        }

        auto Content() const noexcept { return TextContent(); }

        template<std::integral T = LONG>
        auto NumbericContent() const noexcept
        {
            auto Text = TextContent();
            if( ! std::ranges::all_of( Text, ::isdigit ) ) return T{ 0 };

            if constexpr( std::is_signed_v<T> )
                return static_cast<T>( std::stoll( Text ) );
            else
                return static_cast<T>( std::stoull( Text ) );
        }

        auto operator=( std::string_view IncomingContent ) const noexcept { return SetContent( IncomingContent ); }
    };

    struct TextLabelControl : Control
    {
        constexpr TextLabelControl() noexcept
        {
            ClassName = WC_STATIC;
            AddStyle( SS_WORDELLIPSIS );
            RemoveStyle( WS_TABSTOP );
        }

        using Control::operator=;
    };

    struct CanvasControl : Control
    {
        constexpr CanvasControl() noexcept
        {
            ClassName = WC_STATIC;
            Dimension = SIZE{ 0, 0 };
            RemoveStyle( WS_TABSTOP );
        }

        void Paint() const noexcept {}

        void Paint( CanvasContent Content ) const noexcept
        {
            //auto CanvasHandle = Parent.value_or( Handle );
            auto CanvasHandle = Handle;
            if( CanvasHandle )
            {
                CanvasContainer[CanvasHandle] = std::move( Content );
                InvalidateRect( Parent.value_or( CanvasHandle ), NULL, 0 );  // trigger paint event
                //InvalidateRect( CanvasHandle, NULL, 1 );  // trigger paint event
                //RedrawWindow( CanvasHandle, {}, {}, RDW_ERASE | RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN );
                //InvalidateRgn( CanvasHandle, NULL, 1 );  // trigger paint event
            }
        }
    };

    struct EditControl : Control
    {
        constexpr EditControl() noexcept { ClassName = WC_EDIT; }

        //using Control::operator=;
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
        using KeyType = decltype( SendMessage( {}, {}, {}, {} ) );
        using DataContainer = std::vector<std::pair<KeyType, std::string>>;

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
            if( SelectedIndex == LB_ERR ) return LRESULT{ 0 };
            return SendMessage( Handle, LB_GETITEMDATA, SelectedIndex, AlwaysZero );
        }

        auto SelectItem( LRESULT ListIndex ) const noexcept
        {
            if( ListIndex >= SendMessage( Handle, LB_GETCOUNT, AlwaysZero, AlwaysZero ) ) return LRESULT{ LB_ERR };
            return SendMessage( Handle, LB_SETCURSEL, ListIndex, AlwaysZero );
        }

        auto SelectFirstItem() const noexcept { return SelectItem( 0 ); }

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
        constexpr static auto ChildSeparation = 10;

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

            ReSize( { RequiredDimension.cx + 0 * WindowControl::ChildSeparation,
                      RequiredDimension.cy + 0 * WindowControl::ChildSeparation } );

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

        auto Paint( CanvasContent Content ) const noexcept
        {
            if( ! Handle ) return;
            CanvasContainer[Handle] = std::move( Content );
            RedrawWindow( Handle, {}, {},  // RDW_ERASE |
                          RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN );
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

        auto ChildDimension = RHS.Dimension.value_or( SIZE{ 100, 20 } );
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
        if constexpr( ! std::is_const_v<ChildType> )
        {
            RHS.Handle = Handle_;
            RHS.Parent = LHS.Handle;
        }
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
