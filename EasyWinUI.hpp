#ifndef EASYWINUI_H
#define EASYWINUI_H

#include <Windows.h>
#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <thread>
#include <string_view>

#define EVENT_PARAMETER_LIST HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam
namespace EWUI {
    LRESULT CALLBACK WndProc( EVENT_PARAMETER_LIST )
    {
        switch( msg )
        {
            case WM_CREATE: break;
            case WM_COMMAND: return -1;
            case WM_CLOSE: DestroyWindow( hwnd ); break;
            case WM_DESTROY: PostQuitMessage( 0 ); break;
            default: return DefWindowProc( hwnd, msg, wParam, lParam );
        }
        return 0;
    }

    int Main();

    struct tagEntryPointParamPack
    {
        HINSTANCE hInstance;
        HINSTANCE hPrevInstance;
        LPCSTR lpCmdLine;
        int nCmdShow;
    } EntryPointParamPack{};

    struct Message
    {
        MSG InternalMessage{};
    };

    struct ControlConfig
    {
        HWND Handle{};
        HWND Parent{};
        HMENU Menu{};
        LPCSTR ClassName{ "EWUI_Default_ClassName" };
        LPCSTR WindowName{ "EWUI_Default_WindowName" };
        DWORD Style{};
        DWORD ExStyle{};
        POINT Origin{ 0, 0 };
        SIZE Dimension{ 400, 600 };
        void ( *Event )( EVENT_PARAMETER_LIST ){};
    };

    struct Control : ControlConfig
    {};

    struct Window : Control
    {
        Window() : Window( ControlConfig{} ) {}
        Window( ControlConfig _Config ) : Control( _Config )
        {
            auto wc = WNDCLASSEX{ .cbSize = sizeof( WNDCLASSEX ),
                                  .style = CS_HREDRAW | CS_VREDRAW,
                                  .lpfnWndProc = EWUI::WndProc,
                                  .hInstance = EWUI::EntryPointParamPack.hInstance,
                                  .hbrBackground = reinterpret_cast<HBRUSH>( COLOR_WINDOW ),
                                  .lpszClassName = ClassName };
            if( ! RegisterClassEx( &wc ) )
            {
                MessageBox( NULL, "Window Registration Failed!", "Error!", MB_ICONEXCLAMATION | MB_OK );
                Handle = NULL;
                return;
            }

            Handle = CreateWindowEx( WS_EX_LEFT, ClassName, WindowName, WS_OVERLAPPEDWINDOW | WS_TABSTOP,  //
                                     Origin.x, Origin.y, Dimension.cx, Dimension.cy, Parent, Menu,
                                     EWUI::EntryPointParamPack.hInstance, NULL );
            if( Handle == NULL )
            {
                MessageBox( NULL, "Window Creation Failed!", "Error!", MB_ICONEXCLAMATION | MB_OK );
                return;
            }
        }

        operator int()
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
    auto MainWindow = EWUI::Window( { .WindowName = "MyWindow",  //
                                      .Dimension = { 400, 700 } } );
    return MainWindow;
}

#endif