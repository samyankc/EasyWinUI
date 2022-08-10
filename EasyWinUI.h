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

#define PARENS ()

#define EXPAND( ... ) EXPAND4( EXPAND4( EXPAND4( EXPAND4( __VA_ARGS__ ) ) ) )
#define EXPAND4( ... ) EXPAND3( EXPAND3( EXPAND3( EXPAND3( __VA_ARGS__ ) ) ) )
#define EXPAND3( ... ) EXPAND2( EXPAND2( EXPAND2( EXPAND2( __VA_ARGS__ ) ) ) )
#define EXPAND2( ... ) EXPAND1( EXPAND1( EXPAND1( EXPAND1( __VA_ARGS__ ) ) ) )
#define EXPAND1( ... ) __VA_ARGS__

#define FOR_EACH( macro, ... ) __VA_OPT__( EXPAND( FOR_EACH_HELPER( macro, __VA_ARGS__ ) ) )
#define FOR_EACH_HELPER( macro, a1, ... ) macro( a1 ) __VA_OPT__( FOR_EACH_AGAIN PARENS( macro, __VA_ARGS__ ) )
#define FOR_EACH_AGAIN() FOR_EACH_HELPER

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

    struct ControlConfig
    {
        HWND   m_Handle{};
        HWND   m_Parent{};
        HMENU  m_MenuID{};
        LPCSTR m_ClassName{};
        LPCSTR m_Label{};
        DWORD  m_Style{ WS_VISIBLE | WS_CHILD | WS_TABSTOP };
        DWORD  m_ExStyle{};
        POINT  m_Origin{ 0, 0 };
        SIZE   m_Dimension{ 200, 100 };
    };

    template<typename CRTP>
    struct Control : ControlConfig
    {
#define MAKE_GETTER( A ) \
    decltype( auto ) A() const noexcept { return m_##A; }
#define MAKE_SETTER( A )                                  \
    decltype( auto ) A( decltype( m_##A ) A##_ ) noexcept \
    {                                                     \
        m_##A = A##_;                                     \
        return static_cast<CRTP&>( *this );               \
    }
        FOR_EACH( MAKE_GETTER, Handle, Parent, MenuID, ClassName, Label, Style, ExStyle, Origin, Dimension );
        FOR_EACH( MAKE_SETTER, Handle, Parent, MenuID, ClassName, Label, Style, ExStyle, Origin, Dimension );
#undef MAKE_GETTER
#undef MAKE_SETTER

        constexpr operator HWND() const noexcept { return m_Handle; }

        constexpr decltype( auto ) AddStyle( DWORD Style_ ) noexcept
        {
            m_Style |= Style_;
            return static_cast<CRTP&>( *this );
        }

        constexpr decltype( auto ) RemoveStyle( DWORD Style_ ) noexcept
        {
            m_Style &= ~Style_;
            return static_cast<CRTP&>( *this );
        }

        constexpr decltype( auto ) AddExStyle( DWORD ExStyle_ ) noexcept
        {
            m_ExStyle |= ExStyle_;
            return static_cast<CRTP&>( *this );
        }

        constexpr decltype( auto ) RemoveExStyle( DWORD ExStyle_ ) noexcept
        {
            m_ExStyle &= ~ExStyle_;
            return static_cast<CRTP&>( *this );
        }

        auto GetRect() const noexcept
        {
            RECT rect;
            GetWindowRect( Handle(), &rect );
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
            SetWindowPos( Handle(), HWND_NOTOPMOST, 0, 0, NewDimension.cx, NewDimension.cy, SWP_NOMOVE | SWP_NOZORDER );
        }

        auto MoveTo( POINT NewPoint ) const noexcept
        {
            SetWindowPos( Handle(), HWND_NOTOPMOST, NewPoint.x, NewPoint.y, 0, 0, SWP_NOSIZE | SWP_NOZORDER );
        }
    };

    struct TextLabel : Control<TextLabel>
    {
        mutable std::string InternalBuffer;

        TextLabel()
        {
            this->ClassName( WC_STATIC );
            this->RemoveStyle( WS_TABSTOP );
        }

        decltype( auto ) UpdateText() const
        {
            if( this->Handle() == NULL ) return *this;
            SetWindowText( this->Handle(), InternalBuffer.c_str() );
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

    struct Button : Control<Button>
    {
        ControlAction m_Action{};

        Button()
        {
            this->ClassName( WC_BUTTON );
            this->AddStyle( BS_PUSHBUTTON | BS_FLAT );
        }

        decltype( auto ) Action() const noexcept { return m_Action; }

        decltype( auto ) Action( ControlAction Action_ ) noexcept
        {
            m_Action = Action_;
            return *this;
        }
    };


    template<typename CRTP>
    struct EditControl : Control<CRTP>
    {
        EditControl() { this->ClassName( WC_EDIT ); }

        std::string Content()
        {
            if( this->Handle() == NULL ) return "";
            auto RequiredBufferSize = GetWindowTextLength( this->Handle() ) + 1;
            auto ResultString = std::string( RequiredBufferSize, '\0' );
            GetWindowText( this->Handle(), ResultString.data(), RequiredBufferSize );
            return ResultString;
        }

        void operator=( auto&& RHS )
        {
            auto IncomingContent = std::string_view{ RHS };
            SetWindowText( this->Handle(), IncomingContent.data() );
        }
    };

    struct TextBox : EditControl<TextBox>
    {
        using EditControl::operator=;
        TextBox()
        {
            this->AddStyle( ES_AUTOHSCROLL | WS_BORDER );
        }
    };

    struct TextArea : EditControl<TextBox>
    {
        using EditControl::operator=;
        TextArea()
        {
            this->AddStyle( WS_VSCROLL | ES_AUTOVSCROLL | ES_MULTILINE | ES_WANTRETURN );
            this->AddExStyle( WS_EX_CONTROLPARENT );
        }
    };

    template<typename CRTP>
    struct WindowControl : Control<CRTP>
    {
        POINT ComponentOffset{ 10, 0 };
        SIZE  ComponentTotalSize{ 0, 0 };
        // PAINTSTRUCT ps;
        // HDC hdc;

        WindowControl( LPCSTR ClassName_, LPCSTR Label_ )
        {
            this->ClassName( ClassName_ );
            this->Label( Label_ );

            this->RemoveStyle( WS_CHILD | WS_VISIBLE );

            this->AddStyle( WS_OVERLAPPEDWINDOW );
            this->AddExStyle( WS_EX_LEFT | WS_EX_DLGMODALFRAME );  // | WS_EX_LAYERED

            auto wc = WNDCLASSEX{};
            wc.cbSize = sizeof( WNDCLASSEX );
            wc.style = CS_HREDRAW | CS_VREDRAW;
            wc.lpfnWndProc = EWUI::WndProc;
            wc.hInstance = EWUI::EntryPointParamPack.hInstance;
            wc.hbrBackground = reinterpret_cast<HBRUSH>( COLOR_WINDOW );
            wc.lpszClassName = this->ClassName();

            if( ! RegisterClassEx( &wc ) )
            {
                MessageBox( NULL, "Window Registration Failed!", "Error!", MB_ICONEXCLAMATION | MB_OK );
                return;
            }

            this->Handle( CreateWindowEx( this->ExStyle(), this->ClassName(), this->Label(), this->Style(),  //
                                          this->Origin().x, this->Origin().y,                                //
                                          this->Dimension().cx, this->Dimension().cy,                        //
                                          this->Parent(), this->MenuID(), EWUI::EntryPointParamPack.hInstance, NULL ) );
            if( this->Handle() == NULL )
            {
                MessageBox( NULL, "Window Creation Failed!", "Error!", MB_ICONEXCLAMATION | MB_OK );
                return;
            }

            this->Show();
        }

        template<DerivedFrom<ControlConfig> T>
        decltype( auto ) operator<<( T&& Child_ )
        {
            if( this->Handle() == NULL ) return *this;

            Child_.Parent( this->Handle() );
            if( Child_.Handle() == NULL )
            {
                if( Child_.Origin() == POINT{ 0, 0 } ) Child_.Origin( ComponentOffset );
                ComponentOffset.y += Child_.Dimension().cy + 10;
                ComponentTotalSize.cy = ComponentOffset.y;
                ComponentTotalSize.cx = std::max( ComponentTotalSize.cx, Child_.Origin().x + Child_.Dimension().cx );
                Child_.Handle( CreateWindowEx( Child_.ExStyle(), Child_.ClassName(),          //
                                               Child_.Label(), Child_.Style(),                //
                                               Child_.Origin().x, Child_.Origin().y,          //
                                               Child_.Dimension().cx, Child_.Dimension().cy,  //
                                               Child_.Parent(), Child_.MenuID(), EWUI::EntryPointParamPack.hInstance,
                                               NULL ) );
                if constexpr( MatchType<T, Button> )
                {
                    ActionContainer.RegisterAction( Child_.Handle(), Child_.Action() );
                }
            }
            else
            {
                // Change Owner by SetWindowLongPtr( __ , GWLP_HWNDPARENT, __ )
                // SetParent() has different effect
                SetWindowLongPtr( Child_.Handle(), GWLP_HWNDPARENT, reinterpret_cast<LONG_PTR>( Child_.Parent() ) );
            }
            return *this;
        }

        void Show() { ShowWindow( this->Handle(), SW_SHOW ); }

        void Hide() { ShowWindow( this->Handle(), SW_HIDE ); }

        void ToggleVisibility()
        {
            if( GetWindowLong( this->Handle(), GWL_STYLE ) & WS_VISIBLE )
                this->Hide();
            else
                this->Show();
        }
    };

    struct Window : WindowControl<Window>
    {
        Window( LPCSTR ClassName_ = {}, LPCSTR Label_ = {} )
            : WindowControl( ( ClassName_ == NULL || strlen( ClassName_ ) == 0 ) ? "EWUI Window Class" : ClassName_,
                             ( Label_ == NULL || strlen( Label_ ) == 0 ) ? "EWUI Window Title" : Label_ )
        {}

        int Activate() const
        {
            if( this->Handle() == NULL ) return 0;

            if( this->Dimension().cx == -1 && this->Dimension().cy == -1 )
            {
                // buffer space for title bar
                ReSize( { ComponentTotalSize.cx, ComponentTotalSize.cy + 40 } );
            }

            ShowWindow( this->Handle(), EWUI::EntryPointParamPack.nCmdShow );
            UpdateWindow( this->Handle() );

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

                if( ! IsDialogMessage( this->Handle(), &Msg ) )
                {
                    TranslateMessage( &Msg );
                    DispatchMessage( &Msg );
                }
            }
            return static_cast<int>( Msg.wParam );
        }

        operator int() const { return this->Activate(); }
    };

    struct PopupWindow : WindowControl<PopupWindow>
    {
        PopupWindow( LPCSTR ClassName_ = {}, LPCSTR Label_ = {} )
            : WindowControl(
                  ( ClassName_ == NULL || strlen( ClassName_ ) == 0 ) ? "EWUI Popup Window Class" : ClassName_,
                  ( Label_ == NULL || strlen( Label_ ) == 0 ) ? "EWUI Popup Window Title" : Label_ )
        {}
    };

}  // namespace EWUI

int WINAPI WinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow )
{
    EWUI::EntryPointParamPack = { hInstance, hPrevInstance, lpCmdLine, nCmdShow };
    return EWUI::Main();
}
#endif
