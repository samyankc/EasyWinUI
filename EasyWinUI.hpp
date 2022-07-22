#ifndef EASYWINUI_H
#define EASYWINUI_H

#include <Windows.h>
#include <CommCtrl.h>
#include <iostream>
#include <memory>
#include <vector>
#include <string>
#include <algorithm>
#include <thread>
#include <string_view>
#include <optional>
#include <concepts>
#include <functional>
#include <algorithm>

#define EVENT_PARAMETER_LIST HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam

namespace EWUI {

    using ControlAction = std::function<void()>;

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

    int Main();

    struct Message
    {
        MSG InternalMessage{};
    };

    struct tagEntryPointParamPack
    {
        HINSTANCE hInstance;
        HINSTANCE hPrevInstance;
        LPCSTR lpCmdLine;
        int nCmdShow;
    } EntryPointParamPack{};

    struct ControlConfig
    {
        std::optional<HWND> Handle{};
        std::optional<HWND> Parent{};
        std::optional<HMENU> Menu{};
        std::optional<LPCSTR> ClassName{};
        std::optional<LPCSTR> Label{};
        std::optional<DWORD> Style{};
        std::optional<DWORD> ExStyle{};
        std::optional<POINT> Origin{};
        std::optional<SIZE> Dimension{};

        constexpr void operator|=( const ControlConfig& Config_ ) noexcept
        {
            if( Config_.Handle ) Handle = *Config_.Handle;
            if( Config_.Parent ) Parent = *Config_.Parent;
            if( Config_.Menu ) Menu = *Config_.Menu;
            if( Config_.ClassName ) ClassName = *Config_.ClassName;
            if( Config_.Label ) Label = *Config_.Label;
            if( Config_.Style ) Style = *Config_.Style;
            if( Config_.ExStyle ) ExStyle = *Config_.ExStyle;
            if( Config_.Origin ) Origin = *Config_.Origin;
            if( Config_.Dimension ) Dimension = *Config_.Dimension;
        }

    } MainWindowConfig{};

    struct Control : ControlConfig
    {
        Control( const ControlConfig& Config_ )
        {
            Dimension = { 100, 200 };
            *this |= Config_;
        }
        operator HWND() const noexcept { return Handle.value_or( static_cast<HWND>( NULL ) ); }
    };

    struct TextLabel : Control
    {
        TextLabel( const ControlConfig& Config_ ) : Control( Config_ )
        {
            ClassName = WC_STATIC;
            Style = WS_VISIBLE | WS_CHILD;
        }

        void SetText( LPCSTR NewText ) const
        {
            // if( Handle )
            SetWindowText( *Handle, NewText );
        }
    };

    struct Button : Control
    {
        ControlAction Action;

        Button( const ControlConfig& Config_, const ControlAction& Action_ ) : Control( Config_ )
        {
            ClassName = WC_BUTTON;
            Style = WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON;
            Action = Action_;
        }
    };

    struct Window : Control
    {
        // std::vector<std::unique_ptr<Control>> ControlList;

        POINT ComponentOffset{ 10, 0 };

        Window( const ControlConfig& Config_ ) : Control( Config_ )
        {
            if( ! ClassName ) ClassName = "EWUI Window Class";

            auto wc = WNDCLASSEX{ .cbSize = sizeof( WNDCLASSEX ),
                                  .style = CS_HREDRAW | CS_VREDRAW,
                                  .lpfnWndProc = EWUI::WndProc,
                                  .hInstance = EWUI::EntryPointParamPack.hInstance,
                                  .hbrBackground = reinterpret_cast<HBRUSH>( COLOR_WINDOW ),
                                  .lpszClassName = *ClassName };

            if( ! RegisterClassEx( &wc ) )
            {
                MessageBox( NULL, "Window Registration Failed!", "Error!", MB_ICONEXCLAMATION | MB_OK );
                Handle = static_cast<HWND>( NULL );
                return;
            }

            Handle = CreateWindowEx( WS_EX_LEFT, *ClassName, Label.value_or( "EWUI Window Title" ),
                                     WS_OVERLAPPEDWINDOW | WS_TABSTOP,                    //
                                     Origin->x, Origin->y, Dimension->cx, Dimension->cy,  //
                                     Parent.value_or( static_cast<HWND>( NULL ) ),
                                     Menu.value_or( static_cast<HMENU>( NULL ) ),  //
                                     EWUI::EntryPointParamPack.hInstance, NULL );
            if( Handle == static_cast<HWND>( NULL ) )
            {
                MessageBox( NULL, "Window Creation Failed!", "Error!", MB_ICONEXCLAMATION | MB_OK );
                return;
            }
        }

        operator int() const
        {
            if( Handle == static_cast<HWND>( NULL ) ) return 0;
            ShowWindow( *Handle, EWUI::EntryPointParamPack.nCmdShow );
            UpdateWindow( *Handle );
            MSG Msg;
            while( GetMessage( &Msg, NULL, 0, 0 ) > 0 )
            {
                TranslateMessage( &Msg );
                DispatchMessage( &Msg );
            }
            return static_cast<int>( Msg.wParam );
        }

        template<typename T>
        decltype( auto ) operator<<( T&& Control_ )  //
            requires( std::is_base_of_v<Control, std::decay_t<T>> )
        {
            Control_.Parent = *Handle;
            if( ! Control_.Origin ) Control_.Origin = ComponentOffset;
            ComponentOffset.y += Control_.Dimension->cy + 10;
            Control_.Handle = CreateWindow( *Control_.ClassName,                             //
                                            Control_.Label.value_or( "" ),                   //
                                            Control_.Style.value_or( WS_VISIBLE ),           //
                                            Control_.Origin->x, Control_.Origin->y,          //
                                            Control_.Dimension->cx, Control_.Dimension->cy,  //
                                            *Handle, Control_.Menu.value_or( static_cast<HMENU>( NULL ) ),
                                            EWUI::EntryPointParamPack.hInstance, NULL );
            if constexpr( std::is_same_v<Button, std::decay_t<T>> )
            {
                ActionContainer.RegisterAction( *Control_.Handle, Control_.Action );
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
    auto MainWindow = Window( { .Label = "Test Window",  //
                                .Origin = POINT{ 10, 10 },
                                .Dimension = SIZE{ 300, 400 } } );

    auto HeaderLabel = TextLabel( { .Label = "Header, Click button to change this text",  //
                                    .Dimension = SIZE{ 400, 50 } } );

    auto ActionButton = Button( { .Label = "Click Me",  //
                                  .Dimension = SIZE{ 200, 100 } },
                                [ & ] { HeaderLabel.SetText( "Button Clicked." ); } );

    return MainWindow << HeaderLabel                           //
                      << ActionButton                          //
                      << Button( { .Label = "Click Me Again",  //
                                   .Dimension = SIZE{ 200, 120 } },
                                 [ & ] { HeaderLabel.SetText( "Button Clicked Again." ); } );
}

#endif