#ifndef EASYWINUI_H
#define EASYWINUI_H

#include <Windows.h>
#include <CommCtrl.h>
#include <iostream>
#include <iomanip>
#include <memory>
#include <vector>
#include <string>
#include <cstring>
#include <algorithm>
#include <thread>
#include <string_view>
#include <concepts>
#include <functional>
#include <algorithm>

#define EVENT_PARAMETER_LIST HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam

// helpers
namespace
{
    template<typename Derived, typename Base>
    concept DerivedFrom = std::is_base_of_v<Base, std::decay_t<Derived>>;

    template<typename TargetType, typename... CandidateTypes>
    concept MatchExactType = ( std::same_as<TargetType, CandidateTypes> || ... );

    template<typename TargetType, typename... CandidateTypes>
    concept MatchType = MatchExactType<std::decay_t<TargetType>, std::decay_t<CandidateTypes>...>;

    template<MatchType<SIZE, POINT> T>
    bool operator==( const T& LHS, const T& RHS )
    {
        return std::memcmp( &LHS, &RHS, sizeof( T ) ) == 0;
    }


    HWND MonitorHandle = NULL;

    void PrintMSG( std::string_view PrefixString, MSG Msg )
    {
        if( MonitorHandle == NULL || Msg.hwnd != MonitorHandle ) return;
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

    // template<typename T> constexpr LPCSTR ControlClassName = std::decay_t<T>::ClassName;

}  // namespace

namespace EWUI
{
    int Main();

    using ControlAction = std::function<void()>;
    struct ActionContainer_
    {
        inline static ControlAction EmptyAction = [] {};

        std::vector<HWND>          HandleVector;
        std::vector<ControlAction> ActionVector;

        ActionContainer_( std::size_t capactiy = 20 )
        {
            HandleVector.reserve( capactiy );
            ActionVector.reserve( capactiy );
        }

        ControlAction& operator[]( HWND Index )
        {
            auto Position = std::find( HandleVector.begin(), HandleVector.end(), Index );
            if( Position == HandleVector.end() ) return EmptyAction;
            return ActionVector[Position - HandleVector.begin()];
        }

        void RegisterAction( HWND Handle_, MatchType<ControlAction> auto&& Action_ )
        {
            if( Action_ == nullptr ) return;
            HandleVector.push_back( Handle_ );
            ActionVector.push_back( Action_ );
        }

    } ActionContainer{};


    LRESULT CALLBACK WndProc( HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam )
    {
        switch( msg )
        {
            case WM_CREATE : break;
            case WM_COMMAND : std::thread( ActionContainer[reinterpret_cast<HWND>( lParam )] ).detach(); break;
            case WM_CLOSE : DestroyWindow( hwnd ); break;
            case WM_DESTROY :
                if( GetWindow( hwnd, GW_OWNER ) == NULL ) PostQuitMessage( 0 );
                break;
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

    struct ControlConfig
    {
        HWND          Handle{};
        HWND          Parent{};
        HMENU         Menu{};
        LPCSTR        ClassName{};
        LPCSTR        Label{};
        DWORD         Style{};
        DWORD         ExStyle{};
        POINT         Origin{ 0, 0 };
        SIZE          Dimension{ 200, 100 };
        ControlAction Action{};
    } MainWindowConfig{};

    struct Control : ControlConfig
    {
        Control( const ControlConfig& Config_ )
        {
            *this = *reinterpret_cast<const Control*>( &Config_ );
            Style |= WS_VISIBLE | WS_CHILD | WS_TABSTOP;
        }

        operator HWND() const noexcept { return Handle; }

        auto Width() const noexcept { return Dimension.cx; }

        auto Height() const noexcept { return Dimension.cy; }
    };

    struct TextLabel : Control
    {
        mutable std::string InternalBuffer;

        TextLabel( const ControlConfig& Config_ ) : Control( Config_ )
        {
            ClassName = WC_STATIC;
            Style &= ~WS_TABSTOP;
        }

        decltype( auto ) UpdateText() const
        {
            if( Handle == NULL ) return *this;
            SetWindowText( Handle, InternalBuffer.c_str() );
            return *this;
        }

        decltype( auto ) operator=( auto&& NewText ) const
        {
            InternalBuffer = NewText;
            return UpdateText();
        }

        decltype( auto ) operator+=( auto&& NewText ) const
        {
            InternalBuffer += NewText;
            return UpdateText();
        }
    };

    struct Button : Control
    {
        Button( const ControlConfig& Config_ ) : Control( Config_ )
        {
            ClassName = WC_BUTTON;
            Style |= BS_PUSHBUTTON | BS_FLAT;
        }
    };

    struct EditControl : Control
    {
        EditControl( const ControlConfig& Config_ ) : Control( Config_ ) { ClassName = WC_EDIT; }

        std::string Content()
        {
            if( Handle == NULL ) return "";
            auto RequiredBufferSize = GetWindowTextLength( Handle );
            auto ResultString = std::string{};
            ResultString.resize( RequiredBufferSize );
            GetWindowText( Handle, ResultString.data(), RequiredBufferSize );
            return ResultString;
        }

        void operator=( auto&& RHS )
        {
            auto IncomingContent = std::string_view{ RHS };
            SetWindowText( Handle, IncomingContent.data() );
        }
    };

    struct TextBox : EditControl
    {
        TextBox( const ControlConfig& Config_ ) : EditControl( Config_ )
        {
            Style |= ES_AUTOHSCROLL | WS_BORDER;
            Dimension.cy = 24;
        }

        using EditControl::operator=;
        // void operator=( auto&& IncomingContent_ )
        // {
        //     auto IncomingContent = std::string_view{ IncomingContent_ };
        //     SetWindowText( Handle, IncomingContent.data() );
        // }
    };

    struct TextArea : EditControl
    {
        TextArea( const ControlConfig& Config_ ) : EditControl( Config_ )
        {
            Style |= WS_VSCROLL | ES_AUTOVSCROLL | ES_MULTILINE | ES_WANTRETURN;
            ExStyle |= WS_EX_CONTROLPARENT;
        }
    };

    struct WindowControl : Control
    {
        POINT ComponentOffset{ 10, 0 };

        WindowControl( const ControlConfig& Config_, LPCSTR FallbackClassName, LPCSTR FallbackLabel )
            : Control( Config_ )
        {
            if( ClassName == NULL ) ClassName = FallbackClassName;
            if( Label == NULL ) Label = FallbackLabel;

            Style |= WS_OVERLAPPEDWINDOW;
            Style &= ~WS_CHILD;
            ExStyle |= WS_EX_LEFT | WS_EX_DLGMODALFRAME;  // | WS_EX_LAYERED

            auto wc = WNDCLASSEX{};
            wc.cbSize = sizeof( WNDCLASSEX );
            wc.style = CS_HREDRAW | CS_VREDRAW;
            wc.lpfnWndProc = EWUI::WndProc;
            wc.hInstance = EWUI::EntryPointParamPack.hInstance;
            wc.hbrBackground = reinterpret_cast<HBRUSH>( COLOR_WINDOW );
            wc.lpszClassName = ClassName;

            if( ! RegisterClassEx( &wc ) )
            {
                MessageBox( NULL, "Window Registration Failed!", "Error!", MB_ICONEXCLAMATION | MB_OK );
                return;
            }

            // Handle = CreateWindowEx( ExStyle, ClassName, Label, Style,                //
            //                          Origin.x, Origin.y, Dimension.cx, Dimension.cy,  //
            //                          Parent, Menu, EWUI::EntryPointParamPack.hInstance, NULL );
            // if( Handle == NULL )
            // {
            //     MessageBox( NULL, "Window Creation Failed!", "Error!", MB_ICONEXCLAMATION | MB_OK );
            //     return;
            // }
        }

        int Activate() const
        {
            if( Handle == NULL ) return 0;
            ShowWindow( Handle, EWUI::EntryPointParamPack.nCmdShow );
            UpdateWindow( Handle );

            MSG        Msg;
            static int msg_counter = 0;
            while( GetMessage( &Msg, NULL, 0, 0 ) > 0 )
            {
                PrintMSG( ">>", Msg );
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

        operator int() const { return Activate(); }

        template<DerivedFrom<Control> T>
        decltype( auto ) operator<<( T&& Control_ )
        {
            struct PopupWindow;
            if constexpr( MatchType<T, PopupWindow> )
            {
                return *this;
            }

            Control_.Parent = Handle;
            if( Control_.Origin == POINT{ 0, 0 } ) Control_.Origin = ComponentOffset;
            ComponentOffset.y += Control_.Dimension.cy + 10;
            Control_.Handle =
                CreateWindowEx( Control_.ExStyle,                              //
                                Control_.ClassName,                            //
                                Control_.Label, Control_.Style,                //
                                Control_.Origin.x, Control_.Origin.y,          //
                                Control_.Dimension.cx, Control_.Dimension.cy,  //
                                Control_.Parent, Control_.Menu, EWUI::EntryPointParamPack.hInstance, NULL );
            if constexpr( MatchType<T, Button, TextBox> )
            {
                ActionContainer.RegisterAction( Control_.Handle, Control_.Action );
            }
            return *this;
        }
    };

    struct Window : WindowControl
    {
        Window( const ControlConfig& Config_ ) : WindowControl( Config_, "EWUI Window Class", "EWUI Window Title" )
        {
            Handle = CreateWindowEx( ExStyle, ClassName, Label, Style,                //
                                     Origin.x, Origin.y, Dimension.cx, Dimension.cy,  //
                                     Parent, Menu, EWUI::EntryPointParamPack.hInstance, NULL );
            if( Handle == NULL )
            {
                MessageBox( NULL, "Window Creation Failed!", "Error!", MB_ICONEXCLAMATION | MB_OK );
                return;
            }
        }
    };

    struct PopupWindow : WindowControl
    {
        PopupWindow( const ControlConfig& Config_ )
            : WindowControl( Config_, "EWUI Popup Window Class", "EWUI Popup Window Title" )
        {}
    };

}  // namespace EWUI

int WINAPI WinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow )
{
    EWUI::EntryPointParamPack = { hInstance, hPrevInstance, lpCmdLine, nCmdShow };
    return EWUI::Main();
}
#endif

#ifdef TEST_CODE

int EWUI::Main()
{
    auto MainWindow = Window( {
        .Label = "Test Window",
        .Origin = { 10, 10 },
        .Dimension = { 300, 400 },
    } );

    auto HeaderLabel = TextLabel( {
        .Label = "Header, Click button to change this text",
        .Dimension = { 400, 50 },
    } );

    auto ActionButton = Button( {
        .Label = "Click Me",
        .Dimension = { 200, 100 },
        .Action = [&] { HeaderLabel = "Button Clicked."; },
    } );

    return MainWindow << HeaderLabel << ActionButton
                      << Button( {
                             .Label = "Click Me Again",
                             .Dimension = { 200, 120 },
                             .Action = [&] { HeaderLabel = "Button Clicked Again."; },
                         } );
}

#endif