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

namespace EWUI {

    int Main();

    using ControlAction = std::function<void()>;

    template<typename T, typename = std::enable_if_t<std::is_same_v<T, SIZE> ||  //
                                                     std::is_same_v<T, POINT>>>
    bool operator==( const T& LHS, const T& RHS )
    {
        return std::memcmp( &LHS, &RHS, sizeof( T ) ) == 0;
    }

    struct tagActionContainer
    {
        inline static ControlAction EmptyAction = [] {};

        std::vector<HWND> IndexVector;
        std::vector<ControlAction> ActionVector;

        tagActionContainer( std::size_t capactiy )
        {
            IndexVector.reserve( capactiy );
            ActionVector.reserve( capactiy );
        }

        ControlAction& operator[]( HWND Index )
        {
            auto Position = std::find( IndexVector.begin(), IndexVector.end(), Index );
            if( Position == IndexVector.end() ) return EmptyAction;
            return ActionVector[ Position - IndexVector.begin() ];
        }

        template<typename A>
        void RegisterAction( HWND Index_, A&& Action_ )  //
            requires( std::is_same_v<ControlAction, std::decay_t<A>> )
        {
            IndexVector.push_back( Index_ );
            ActionVector.push_back( Action_ );
        }

    } ActionContainer{ 20 };

    LRESULT CALLBACK WndProc( HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam )
    {
        switch( msg )
        {
            case WM_CREATE: break;
            case WM_COMMAND: std::thread( ActionContainer[ reinterpret_cast<HWND>( lParam ) ] ).detach(); break;
            case WM_CLOSE: DestroyWindow( hwnd ); break;
            case WM_DESTROY: PostQuitMessage( 0 ); break;
            default: return DefWindowProc( hwnd, msg, wParam, lParam );
        }
        return 0;
    }

    struct tagEntryPointParamPack
    {
        HINSTANCE hInstance;
        HINSTANCE hPrevInstance;
        LPCSTR lpCmdLine;
        int nCmdShow;
    } EntryPointParamPack{};

    template<typename T>
    constexpr LPCSTR ControlClassName = std::decay_t<T>::ClassName;

    struct ControlConfig
    {
        HWND Handle{};
        HWND Parent{};
        HMENU Menu{};
        LPCSTR ClassName{};
        LPCSTR Label{};
        DWORD Style{};
        DWORD ExStyle{};
        POINT Origin{ 0, 0 };
        SIZE Dimension{ 200, 100 };
        ControlAction Action{};
    } MainWindowConfig{};

    struct Control : ControlConfig
    {
        Control( const ControlConfig& Config_ ) { *this = *reinterpret_cast<const Control*>( &Config_ ); }
        operator HWND() const noexcept { return Handle; }
        auto Width() const noexcept { return Dimension.cx; }
        auto Height() const noexcept { return Dimension.cy; }
    };

    struct TextLabel : Control
    {
        constexpr static LPCSTR ClassName = WC_STATIC;

        mutable std::string InternalBuffer;

        TextLabel( const ControlConfig& Config_ ) : Control( Config_ ) { Style |= WS_VISIBLE | WS_CHILD; }

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
        Button( const ControlConfig& Config_ ) : Control( Config_ ) { Style |= WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON; }
    };

    struct Window : Control
    {
        POINT ComponentOffset{ 10, 0 };

        Window( const ControlConfig& Config_ ) : Control( Config_ )
        {
            if( ClassName == NULL ) ClassName = "EWUI Window Class";
            if( Label == NULL ) Label = "EWUI Window Title";

            auto wc = WNDCLASSEX{ .cbSize = sizeof( WNDCLASSEX ),
                                  .style = CS_HREDRAW | CS_VREDRAW,
                                  .lpfnWndProc = EWUI::WndProc,
                                  .hInstance = EWUI::EntryPointParamPack.hInstance,
                                  .hbrBackground = reinterpret_cast<HBRUSH>( COLOR_WINDOW ),
                                  .lpszClassName = ClassName };

            if( ! RegisterClassEx( &wc ) )
            {
                MessageBox( NULL, "Window Registration Failed!", "Error!", MB_ICONEXCLAMATION | MB_OK );
                return;
            }

            Handle = CreateWindowEx( WS_EX_LEFT, ClassName, Label,                    //
                                     WS_OVERLAPPEDWINDOW | WS_TABSTOP,                //
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
            MSG Msg;
            while( GetMessage( &Msg, NULL, 0, 0 ) > 0 )
            {
                TranslateMessage( &Msg );
                DispatchMessage( &Msg );
            }
            return static_cast<int>( Msg.wParam );
        }

        operator int() const { return Activate(); }

        template<typename T>
        decltype( auto ) operator<<( T&& Control_ )  //
            requires( std::is_base_of_v<Control, std::decay_t<T>> )
        {
            Control_.Parent = Handle;
            if( Control_.Origin == POINT{ 0, 0 } ) Control_.Origin = ComponentOffset;
            ComponentOffset.y += Control_.Dimension.cy + 10;
            Control_.Handle = CreateWindow( ControlClassName<T>,                           //
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
        .Action = [ & ] { HeaderLabel = "Button Clicked."; },
    } );

    return MainWindow << HeaderLabel << ActionButton
                      << Button( {
                             .Label = "Click Me Again",
                             .Dimension = { 200, 120 },
                             .Action = [ & ] { HeaderLabel = "Button Clicked Again."; },
                         } );
}

#endif