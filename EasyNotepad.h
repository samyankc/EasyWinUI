#ifndef EASYNOTEPAD_H
#define EASYNOTEPAD_H

#include <windows.h>

#include <cstring>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

struct Notepad_
{
    struct exe_
    {
        const char* AppName;
        const char* EditName;
        LRESULT ( *SendMessageProxy )( HWND, UINT, WPARAM, LPARAM ){ SendMessage };
        HWND AppHandle{ NULL };
        PROCESS_INFORMATION ProcessInfo{};

        auto& operator<<( const std::string& IncomingText )
        {
            if( AppName == NULL ) return *this;
            if( AppHandle == NULL )
            {
                STARTUPINFO StartUpInfo;
                ZeroMemory( &StartUpInfo, sizeof( StartUpInfo ) );

                StartUpInfo.dwFlags = STARTF_USESHOWWINDOW | STARTF_FORCEOFFFEEDBACK;
                StartUpInfo.wShowWindow = SW_SHOWNA;

                ZeroMemory( &ProcessInfo, sizeof( ProcessInfo ) );
                StartUpInfo.cb = sizeof( StartUpInfo );
                if( CreateProcess( AppName, NULL, NULL, NULL, false, 0, NULL, NULL, &StartUpInfo, &ProcessInfo ) )
                {
                    Sleep( 100 );  // wait for process to exist, at least 30 ms
                    do {
                        AppHandle = FindWindowEx( NULL, AppHandle, NULL, NULL );
                        if( GetWindowThreadProcessId( AppHandle, NULL ) == ProcessInfo.dwThreadId )
                            if( GetParent( AppHandle ) == NULL ) break;
                    } while( AppHandle != NULL );
                    AppHandle = FindWindowEx( AppHandle, NULL, EditName, NULL );
                }
            }

            if( IsWindow( AppHandle ) )
            {
                auto NewTextLength = SendMessage( AppHandle, WM_GETTEXTLENGTH, 0, 0 ) + IncomingText.length() + 1;
                auto NewText = std::string(NewTextLength,'\0');
                LRESULT JumpStartIndex = SendMessage( AppHandle, WM_GETTEXT, NewTextLength, reinterpret_cast<LPARAM>(NewText.data()) );
                memcpy( NewText.data() + JumpStartIndex, IncomingText.data(), IncomingText.length() + 1 );
                SendMessageProxy( AppHandle, WM_SETTEXT, 0, reinterpret_cast<LPARAM>(NewText.data()) );
            }
            else
                std::cout << IncomingText;

            return *this;
        }

        auto& operator<<( const auto& IncomingContent )
        {
            std::ostringstream OSS;
            OSS << IncomingContent;
            return ( *this ) << OSS.str();
        }

        void operator>>( const char* MessageSink ) { AppName = MessageSink == NULL ? NULL : AppName; }
        operator const char*() { return AppName; }

        ~exe_()
        {
            if( AppHandle == NULL ) return;
            std::cout << "\n>>\n>> Waiting for termination >>  " << AppName << "\n>>" << std::flush;
            WaitForSingleObject( ProcessInfo.hProcess, INFINITE );
            CloseHandle( ProcessInfo.hProcess );
            CloseHandle( ProcessInfo.hThread );
        }

    } exe;

    Notepad_& operator++( int );
} Notepad{ "C:\\Windows\\System32\\notepad.exe", "Edit", SendMessage },
    NotepadPlusPlus{ "C:\\Program Files\\Notepad++\\notepad++.exe", "Scintilla", SendMessageW };
Notepad_& Notepad_::operator++( int ) { return NotepadPlusPlus; }

#endif
