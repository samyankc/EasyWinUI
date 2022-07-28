#ifndef EASYWINUI_H
#define EASYWINUI_H

#include <Windows.h>
#include <CommCtrl.h>
#include <iostream>
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

// helper functions
namespace
{
    template<typename T>
        requires( std::same_as<T, SIZE> || std::same_as<T, POINT> )
    bool operator==( const T& LHS, const T& RHS ) { return std::memcmp( &LHS, &RHS, sizeof( T ) ) == 0; }
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

        template<typename A>
            requires( std::is_same_v<ControlAction, std::decay_t<A>> )
        void RegisterAction( HWND Handle_, A&& Action_ )
        {
            HandleVector.push_back( Handle_ );
            ActionVector.push_back( Action_ );
        }

    } ActionContainer{};



    LRESULT CALLBACK WndProc( HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam )
    {

        std::cout<< msg <<"\t" << HIWORD(wParam) <<"\t"<<LOWORD(wParam)<< "\t"<<lParam << std::endl;

        switch( msg )
        {
            case WM_CREATE : break;
            case WM_COMMAND : std::thread( ActionContainer[reinterpret_cast<HWND>( lParam )] ).detach(); break;
            case WM_CLOSE : DestroyWindow( hwnd ); break;
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

    template<typename T>
    constexpr LPCSTR ControlClassName = std::decay_t<T>::ClassName;

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
        constexpr static LPCSTR ClassName = WC_STATIC;

        mutable std::string InternalBuffer;

        TextLabel( const ControlConfig& Config_ ) : Control( Config_ ) { Style &= ~WS_TABSTOP; }

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
        constexpr static LPCSTR ClassName = WC_BUTTON;

        Button( const ControlConfig& Config_ ) : Control( Config_ ) { Style |= BS_PUSHBUTTON; }
    };

    struct EditControl : Control
    {
        constexpr static LPCSTR ClassName = WC_EDIT;

        EditControl( const ControlConfig& Config_ ) : Control( Config_ ) {}

        std::string Content()
        {
            if( Handle == NULL ) return "";
            auto RequiredBufferSize = GetWindowTextLength( Handle );
            auto ResultString = std::string{};
            ResultString.resize( RequiredBufferSize );
            GetWindowText( Handle, ResultString.data(), RequiredBufferSize );
            return ResultString;
        }
    };

    struct TextBox : EditControl
    {
        TextBox( const ControlConfig& Config_ ) : EditControl( Config_ )
        {
            Style |= ES_AUTOHSCROLL | WS_BORDER;
            Dimension.cy = 24;
        }
    };

    struct TextArea : EditControl
    {
        TextArea( const ControlConfig& Config_ ) : EditControl( Config_ )
        {
            Style |= WS_VSCROLL | ES_AUTOVSCROLL | ES_MULTILINE | ES_WANTRETURN;
        }
    };

    struct Window : Control
    {
        POINT ComponentOffset{ 10, 0 };

        Window( const ControlConfig& Config_ ) : Control( Config_ )
        {
            if( ClassName == NULL ) ClassName = "EWUI Window Class";
            if( Label == NULL ) Label = "EWUI Window Title";

            Style |= WS_OVERLAPPEDWINDOW;
            Style &= ~WS_CHILD;
            ExStyle |= WS_EX_LEFT | WS_EX_DLGMODALFRAME;

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

            Handle = CreateWindowEx( ExStyle, ClassName, Label, Style,                //
                                     Origin.x, Origin.y, Dimension.cx, Dimension.cy,  //
                                     Parent, Menu, EWUI::EntryPointParamPack.hInstance, NULL );
            if( Handle == NULL )
            {
                MessageBox( NULL, "Window Creation Failed!", "Error!", MB_ICONEXCLAMATION | MB_OK );
                return;
            }
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
                TranslateMessage( &Msg );
                DispatchMessage( &Msg );
            }
            return static_cast<int>( Msg.wParam );
        }

        operator int() const { return Activate(); }

        template<typename T>
            requires( std::is_base_of_v<Control, std::decay_t<T>> )
        decltype( auto ) operator<<( T&& Control_ )
        {
            Control_.Parent = Handle;
            if( Control_.Origin == POINT{ 0, 0 } ) Control_.Origin = ComponentOffset;
            ComponentOffset.y += Control_.Dimension.cy + 10;
            Control_.Handle = CreateWindowEx( Control_.ExStyle,                              //
                                              ControlClassName<T>,                           //
                                              Control_.Label, Control_.Style,                //
                                              Control_.Origin.x, Control_.Origin.y,          //
                                              Control_.Dimension.cx, Control_.Dimension.cy,  //
                                              Handle, Control_.Menu, EWUI::EntryPointParamPack.hInstance, NULL );
            if constexpr( std::is_same_v<Button, std::decay_t<T>> )
            {
                ActionContainer.RegisterAction( Control_.Handle, Control_.Action );
            }
            return *this;
        }
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
        .Origin = { 10,  10},
        .Dimension = {300, 400},
    } );

    auto HeaderLabel = TextLabel( {
        .Label = "Header, Click button to change this text",
        .Dimension = {400, 50},
    } );

    auto ActionButton = Button( {
        .Label = "Click Me",
        .Dimension = {200, 100 },
        .Action = [&] { HeaderLabel = "Button Clicked.";     },
    } );

    return MainWindow << HeaderLabel << ActionButton
                      << Button( {
                             .Label = "Click Me Again",
                             .Dimension = {200, 120 },
                             .Action = [&] { HeaderLabel = "Button Clicked Again.";     },
    } );
}

#endif