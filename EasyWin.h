#ifndef EASYWIN_H
#define EASYWIN_H

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
#include <queue>
#include <mutex>
#include <winnt.h>
#include <chrono>
#include <utility>
#include <cstdint>
#include <gdiplus.h>
#include <iterator>
#include <ratio>
#include <bit>
#include <ios>
#include <span>
#include <windef.h>
#include <wingdi.h>

//#define EVENT_PARAMETER_LIST HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam

// helpers
namespace
{

    constexpr inline auto MaxClassNameLength = 256uz;
    constexpr inline auto SIMILARITY_THRESHOLD = 30;

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
        char Buffer[BufferSize];
        GetClassName( Handle, Buffer, BufferSize );
        return std::string_view{ Buffer } == TargetClassName;
    }

}  // namespace

namespace EW
{
    int Main();

    using Clock = std::chrono::high_resolution_clock;
    template<std::invocable Callable>
    [[nodiscard]] auto Debounce( Clock::duration DebounceAmplitude, Callable&& Action )
    {
        struct DebounceAction
        {
            Callable Action;
            Clock::duration DebounceAmplitude;
            std::atomic<Clock::time_point> ScheduledInvokeTime;

            DebounceAction( Callable&& Action, Clock::duration DebounceAmplitude )
                : Action{ std::move( Action ) }, DebounceAmplitude{ DebounceAmplitude }
            {}

            DebounceAction( const DebounceAction& Source )
                : DebounceAction( auto{ Source.Action }, Source.DebounceAmplitude )
            {}

            auto operator()()
            {
                if( ScheduledInvokeTime.exchange( Clock::now() + DebounceAmplitude ) == Clock::time_point{} )
                {
                    std::thread( [&] {
                        while( true )
                        {
                            std::this_thread::sleep_until( ScheduledInvokeTime.load() );
                            auto CurrentSchedule = ScheduledInvokeTime.load();
                            if( Clock::now() >= CurrentSchedule &&  //
                                ScheduledInvokeTime.compare_exchange_strong( CurrentSchedule, {} ) )
                            {
                                break;
                            }
                        }
                        Action();
                    } ).detach();
                }
            }
        };

        return DebounceAction{ std::move( Action ), DebounceAmplitude };
    }

    template<std::invocable Callable>
    [[nodiscard]] auto Debounce( Callable&& Action )
    {
        return Debounce( std::chrono::milliseconds( 150 ), std::forward<Callable>( Action ) );
    }

    struct ThreadPool
    {
        using TaskType = std::function<void()>;
        using TaskQueueType = std::queue<TaskType>;
        struct Worker
        {
            TaskType Task;
            std::atomic<bool> Idle;
            std::atomic<bool> Spinning;

            Worker() : Idle{ true }, Spinning{ true }
            {
                std::thread( [this] {
                    while( Spinning )
                    {
                        Idle.wait( true, std::memory_order_acquire );
                        if( Task ) Task();
                        Idle.store( true, std::memory_order_release );
                    }
                } ).detach();
            }

            void Activate()
            {
                Idle.store( false, std::memory_order_release );
                Idle.notify_one();
            }

            bool Available()
            {
                if( Idle.load( std::memory_order_acquire ) ) return true;
                Idle.notify_one();
                return false;
            }

            ~Worker()
            {
                Spinning.store( false );
                Task = nullptr;
                if( Idle ) Activate();
            }
        };

        constexpr static auto ThreadCount = 4uz;

        inline static auto Workers = std::vector<Worker>( ThreadCount );

        inline static auto TaskQueue = TaskQueueType{};
        inline static auto TaskQueueFilled = std::atomic<bool>{};
        inline static auto TaskQueueMutex = std::mutex{};
        inline static auto TaskDistributor =
            ( std::thread( [] {
                  while( true )
                  {
                      TaskQueueFilled.wait( false, std::memory_order_acquire );

                      auto IdleWorker = std::ranges::find_if( Workers, &Worker::Available );

                      if( IdleWorker != Workers.end() )
                      {
                          auto& W = *IdleWorker;
                          W.Task = std::move( TaskQueue.front() );
                          W.Activate();
                          {
                              std::scoped_lock Lock( TaskQueueMutex );
                              TaskQueue.pop();
                              if( TaskQueue.empty() ) TaskQueueFilled.store( false, std::memory_order_release );
                          }
                      }
                  }
              } ).detach(),
              0 );

        static void WaitComplete()
        {
            while( ! TaskQueue.empty() )
            {
                TaskQueueFilled.notify_one();
                std::this_thread::yield();
            }
            while( ! std::ranges::all_of( Workers, &Worker::Available ) ) std::this_thread::yield();
        }

        static void Execute( TaskType NewTask )
        {
            std::scoped_lock Lock( TaskQueueMutex );
            TaskQueue.push( std::move( NewTask ) );
            TaskQueueFilled.store( true, std::memory_order_release );
            TaskQueueFilled.notify_one();
        }
    };

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
    inline std::map<HWND, ControlAction> OnChangeEventContainer;

    using namespace std::chrono;
    using namespace std::literals::chrono_literals;
    struct CancellableThread
    {
        time_point<steady_clock> ExecutionTimePoint;
        std::thread Thread;
        std::atomic_flag CancellationToken;
    };
    inline std::map<HWND, CancellableThread> OnChangeEventThreadContainer;

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
                auto ControlHandle = std::bit_cast<HWND>( lParam );
                auto ControlIdentifier = LOWORD( wParam );
                auto NotificationCode = HIWORD( wParam );
                switch( NotificationCode )
                {
                    case BN_CLICKED :
                    {
                        if( ActionContainer.contains( ControlHandle ) )
                            //std::thread( ActionContainer[ControlHandle] ).detach();
                            ThreadPool::Execute( ActionContainer[ControlHandle] );
                        break;
                    }
                    case EN_CHANGE :
                    {
                        //std::cout << "EN_CHANGE" << std::endl;
                        auto& CurrentEvent = OnChangeEventThreadContainer[ControlHandle];

                        // if thread not started
                        if( CurrentEvent.ExecutionTimePoint == time_point<steady_clock>{} )
                        {
                            CurrentEvent.ExecutionTimePoint = steady_clock::now() + 700ms;
                            std::thread( [ControlHandle = ControlHandle,
                                          &ExecutionTimePoint = CurrentEvent.ExecutionTimePoint] {
                                while( steady_clock::now() < ExecutionTimePoint ) std::this_thread::yield();
                                std::cout << "EN_CHANGE Handled  " << GetWindowTextLength( ControlHandle ) << std::endl;
                                ExecutionTimePoint = {};  // time_point<steady_clock>{};
                            } ).detach();
                        }
                        else
                        {
                            CurrentEvent.ExecutionTimePoint = steady_clock::now() + 400ms;
                            std::cout << "EN_CHANGE Skipped" << std::endl;
                        }
                        break;
                    }
                    default :
                        // std::cout << "ControlHandle: \t" << ControlHandle << std::endl;
                        // std::cout << "ControlIdentifier: \t" << ControlIdentifier << std::endl;
                        // std::cout << "NotificationCode: \t" << NotificationCode << std::endl;
                        break;
                }
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
        LPCSTR lpCmdLine;
        int nCmdShow;
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
        std::optional<HWND> Parent;
        std::optional<HMENU> MenuID;
        std::optional<LPCSTR> ClassName;
        std::optional<LPCSTR> Label;
        std::optional<DWORD> Style;
        std::optional<DWORD> ExStyle;
        std::optional<POINT> Origin;
        std::optional<SIZE> Dimension;
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

    struct OnChangeEvent
    {
        std::chrono::milliseconds Debounce;
        ControlAction Action;
        OnChangeEvent( const auto& Debounce, const auto& Action ) : Debounce{ Debounce }, Action{ Action } {}
    };

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

        auto Reset() const noexcept { return SendMessage( Handle, LB_RESETCONTENT, AlwaysZero, AlwaysZero ); }

        void AddItem( auto Key, std::string_view DisplayString ) const noexcept
        {
            if( ! Handle ) return;
            auto ListIndex =
                SendMessage( Handle, LB_ADDSTRING, AlwaysZero, std::bit_cast<LPARAM>( DisplayString.data() ) );
            SendMessage( Handle, LB_SETITEMDATA, ListIndex, std::bit_cast<LPARAM>( Key ) );
        };

        void SetContent( const DataContainer& Source ) const noexcept
        {
            if( ! Handle ) return;
            Reset();
            for( auto&& [Key, DisplayString] : Source ) AddItem( Key, DisplayString );
        };

        auto Selection() const noexcept
        {
            auto SelectedIndex = SendMessage( Handle, LB_GETCURSEL, AlwaysZero, AlwaysZero );
            if( SelectedIndex == LB_ERR ) return SelectedIndex;
            return SendMessage( Handle, LB_GETITEMDATA, SelectedIndex, AlwaysZero );
        }

        auto SelectItem( LRESULT ListIndex ) const noexcept
        {
            if( ListIndex >= SendMessage( Handle, LB_GETCOUNT, AlwaysZero, AlwaysZero ) ) return LRESULT{ LB_ERR };
            return SendMessage( Handle, LB_SETCURSEL, ListIndex, AlwaysZero );
        }

        auto SelectFirstItem() const noexcept { return SelectItem( 0 ); }
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

        SIZE RequiredDimension{};
        POINT AnchorOffset{ ChildSeparation, ChildSeparation };

        static HWND NewWindow( LPCSTR ClassName_, LPCSTR WindowTitle_ )
        {
            HWND Handle;
            auto wc = WNDCLASSEX{};
            wc.cbSize = sizeof( WNDCLASSEX );
            wc.style = CS_HREDRAW | CS_VREDRAW;
            wc.lpfnWndProc = EW::WndProc;
            wc.hInstance = EW::EntryPointParamPack.hInstance;
            wc.hbrBackground = reinterpret_cast<HBRUSH>( COLOR_WINDOW );
            wc.lpszClassName = ClassName_;

            if( ! RegisterClassEx( &wc ) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS )
                return MessageBox( NULL, "Window Registration Failed!", "Error!", MB_ICONEXCLAMATION | MB_OK ), nullptr;

            Handle = CreateWindowEx( WS_EX_LEFT | WS_EX_DLGMODALFRAME,  //
                                     ClassName_, WindowTitle_,          //
                                     WS_OVERLAPPEDWINDOW,               //
                                     0, 0, 0, 0,                        //
                                     NULL, NULL, EW::EntryPointParamPack.hInstance, NULL );
            if( ! Handle )
                return MessageBox( NULL, "Window Creation Failed!", "Error!", MB_ICONEXCLAMATION | MB_OK ), nullptr;

            return Handle;
        }

        constexpr WindowControl() {}

        WindowControl( LPCSTR ClassName_, LPCSTR WindowName_ ) { Handle = NewWindow( ClassName_, WindowName_ ); }

        WindowControl( LPCSTR ClassName_ ) : WindowControl( ClassName_, "EW Window" ) {}

        WindowControl( const WindowControl& Source )
        {
            Handle = Source.Handle ? Source.Handle : NewWindow( "EW Window Class", "EW Window" );
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

            ShowWindow( Handle, EW::EntryPointParamPack.nCmdShow );
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
        PopupWindowControl( LPCSTR ClassName_ ) : WindowControl( ClassName_, "EW Popup Window" ) {}
        PopupWindowControl( const PopupWindowControl& Source )
        {
            Handle = Source.Handle ? Source.Handle : NewWindow( "EW Popup Window Class", "EW Popup Window" );
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
                                       LHS.Handle, Read( RHS.MenuID ), EW::EntryPointParamPack.hInstance, NULL );

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

    constexpr auto IGNORE_PIXEL = decltype( RGBQUAD::rgbReserved ){ 1 };
    constexpr auto REJECT_COLOUR = decltype( RGBQUAD::rgbReserved ){ 2 };
    constexpr auto IGNORE_TEXT = "_WwWwWwW_";

    constexpr auto nMaxCount = 256uz;

    inline auto ShowHandleName( HWND Handle )
    {
        char TextBuffer[nMaxCount];

        std::cout << std::left << std::setw( 13 )  //
                  << std::hex << std::showbase << ( Handle ) << std::dec;

        GetClassName( Handle, TextBuffer, nMaxCount );
        std::cout << std::setw( 32 ) << std::string( "[ " ) + TextBuffer + " ]" << ' ';

        GetWindowText( Handle, TextBuffer, nMaxCount );
        std::cout << "[ " << TextBuffer << " ] \n";
    }

    inline void ShowAllChild( HWND Handle )
    {
        EnumChildWindows( Handle,
                          []( HWND ChildHandle, LPARAM ) -> WINBOOL {
                              std::cout << "\\ ";
                              ShowHandleName( ChildHandle );
                              return true;
                          },
                          {} );
    }

    inline HWND ObtainFocusHandle( DWORD Delay = 2000 )
    {
        Sleep( Delay );
        if( HWND ret = GetFocus(); ret ) return ret;
        return GetForegroundWindow();
    }

    constexpr auto operator"" _VK( char ch )
    {
        if( ch >= 'a' && ch <= 'z' ) ch -= 'a' - 'A';
        return static_cast<WPARAM>( ch );
    }

    inline auto GetWindowHandleAll( HWND ParentHandle = nullptr )
    {
        std::vector<HWND> Handles;
        Handles.reserve( 256 );

        EnumChildWindows(
            ParentHandle,
            []( HWND Handle, LPARAM lParam_ ) -> WINBOOL {
                reinterpret_cast<std::vector<HWND>*>( lParam_ )->push_back( Handle );
                return true;
            },
            reinterpret_cast<LPARAM>( &Handles ) );

        return Handles;
    }

    inline auto GetWindowHandleByName( std::string_view Name )
    {
        auto Handles = GetWindowHandleAll();
        auto NotFoundBy = [=]( auto... NameExtractors ) {
            return [=]( HWND Handle ) {
                auto FoundBy = [=]( auto NameExtractor ) {
                    char TextBuffer[nMaxCount];
                    NameExtractor( Handle, TextBuffer, nMaxCount );
                    return std::string_view( TextBuffer ).contains( Name );
                };
                return ! ( FoundBy( NameExtractors ) || ... );
            };
        };
        std::erase_if( Handles, NotFoundBy( GetClassName, GetWindowText ) );
        return Handles;
    }

    inline auto GetWindowHandleByName_( std::string_view Name )
    {
        auto Handles = GetWindowHandleAll();
        auto Results = decltype( Handles ){};
        std::ranges::copy_if( Handles, std::back_inserter( Results ),  //
                              [=]( HWND Handle ) {
                                  auto FoundBy = [=]( auto NameExtractor ) {
                                      char TextBuffer[nMaxCount];
                                      NameExtractor( Handle, TextBuffer, nMaxCount );
                                      return std::string_view( TextBuffer ).contains( Name );
                                  };
                                  return FoundBy( GetClassName ) || FoundBy( GetWindowText );
                              } );
        return Results;
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

    inline auto ClassNameOfHandle( HWND Handle )
    {
        std::string ResultString;
        ResultString.resize_and_overwrite( 256uz, std::bind_front( GetWindowText, Handle ) );
        //ResultString.resize_and_overwrite( 256uz, std::bind_front( GetClassName, Handle ) ); // ?
        return ResultString;
    }

    inline bool Similar( RGBQUAD LHS, RGBQUAD RHS, int SimilarityThreshold = SIMILARITY_THRESHOLD )
    {
        constexpr auto Span = []( auto& S ) { return std::span( reinterpret_cast<const BYTE( & )[4]>( S ) ); };
        return [=]( auto... i ) {
            return ( ( std::abs( Span( LHS )[i] - Span( RHS )[i] ) <= SimilarityThreshold ) & ... );
        }( 1, 2, 3 );
    }

    // struct Point
    // {
    //     LPARAM POINT_DATA;
    //     constexpr Point( LPARAM _src ) : POINT_DATA( _src ) {}
    //     constexpr Point( int x, int y ) : POINT_DATA( MAKELPARAM( x, y ) ) {}
    //     constexpr         operator LPARAM&() { return POINT_DATA; }
    //     constexpr Point   operator-() const { return { -x(), -y() }; }
    //     constexpr Point   operator<<=( const Point& _offset ) const { return { x() + _offset.x(), y() + _offset.y() }; }
    //     constexpr Point&  operator+=( const Point& _offset ) { return *this = { x() + _offset.x(), y() + _offset.y() }; }
    //     constexpr int16_t x() const { return LOWORD( POINT_DATA ); }
    //     constexpr int16_t y() const { return HIWORD( POINT_DATA ); }
    //     constexpr friend bool operator==( const Point& lhs, const Point& rhs ) { return lhs.POINT_DATA == rhs.POINT_DATA; }
    // };
    // using Offset = Point;

    constexpr auto operator+( const POINT& LHS, const POINT& RHS ) { return POINT{ LHS.x + RHS.x, LHS.y + RHS.y }; }
    constexpr auto operator-( const POINT& LHS, const POINT& RHS ) { return POINT{ LHS.x - RHS.x, LHS.y - RHS.y }; }
    constexpr auto operator==( const POINT& LHS, const POINT& RHS ) { return LHS.x == RHS.x && LHS.y == RHS.y; }
    constexpr auto operator==( const SIZE& LHS, const SIZE& RHS ) { return LHS.cx == RHS.cx && LHS.cy == RHS.cy; }

    constexpr auto& operator+=( POINT& LHS, const SIZE& RHS )
    {
        LHS.x += RHS.cx;
        LHS.y += RHS.cy;
        return LHS;
    }

    constexpr auto& operator-=( POINT& LHS, const SIZE& RHS )
    {
        LHS.x -= RHS.cx;
        LHS.y -= RHS.cy;
        return LHS;
    }

    constexpr auto Int_To_RGBQUAD( unsigned int n ) noexcept { return std::bit_cast<RGBQUAD>( n ); }

    constexpr auto RGBQUAD_To_Int( RGBQUAD c ) noexcept { return std::bit_cast<unsigned int>( c ); }

    constexpr auto operator|( RGBQUAD LHS, RGBQUAD RHS ) noexcept
    {
        return Int_To_RGBQUAD( RGBQUAD_To_Int( LHS ) | RGBQUAD_To_Int( RHS ) );
    }

    constexpr auto operator&( RGBQUAD LHS, RGBQUAD RHS ) noexcept
    {
        return Int_To_RGBQUAD( RGBQUAD_To_Int( LHS ) & RGBQUAD_To_Int( RHS ) );
    }

    constexpr auto operator==( RGBQUAD LHS, RGBQUAD RHS ) noexcept
    {
        return RGBQUAD_To_Int( LHS ) == RGBQUAD_To_Int( RHS );
    }

    struct EasyBitMap
    {
        POINT Origin;
        SIZE Dimension;
        BITMAPINFO BitMapInformation{};
        std::vector<RGBQUAD> Pixels;

        EasyBitMap() = default;
        EasyBitMap( POINT Origin_, SIZE Dimension_ ) : Origin( Origin_ ), Dimension( Dimension_ )
        {
            BitMapInformation.bmiHeader.biSize = sizeof( BITMAPINFOHEADER );
            BitMapInformation.bmiHeader.biWidth = Dimension.cx;
            BitMapInformation.bmiHeader.biHeight = -Dimension.cy;
            BitMapInformation.bmiHeader.biPlanes = 1;
            BitMapInformation.bmiHeader.biBitCount = 32;
            BitMapInformation.bmiHeader.biSizeImage = 0;
            BitMapInformation.bmiHeader.biCompression = BI_RGB;

            Pixels.resize( Dimension.cx * Dimension.cy );
        }

        auto IsolateColour( RGBQUAD KeepColour, int SimilarityThreshold = SIMILARITY_THRESHOLD ) const
        {
            EasyBitMap NewMap{ *this };
            for( auto&& CurrentPixel : NewMap.Pixels )
                if( ! Similar( CurrentPixel, KeepColour, SimilarityThreshold ) )
                {
                    CurrentPixel = { .rgbReserved = IGNORE_PIXEL };
                }
            // else CurrentPixel = KeepColour;
            return NewMap;
        }

        auto ExtractByColour( RGBQUAD TargetColour, int SimilarityThreshold = SIMILARITY_THRESHOLD ) const
        {
            EasyBitMap NewMap{ *this };
            for( auto&& CurrentPixel : NewMap.Pixels )
                if( ! Similar( CurrentPixel, TargetColour, SimilarityThreshold ) )
                    CurrentPixel = { .rgbReserved = IGNORE_PIXEL };
            return NewMap;
        }

        auto GetColour( int x, int y ) { return Pixels[x + y * Dimension.cx]; }

        auto& operator[]( const POINT& CheckPoint ) { return Pixels[CheckPoint.x + CheckPoint.y * Dimension.cx]; }
        //auto& operator[]( const LONG x, const LONG y ) { return Pixels[x + y * Dimension.cx]; }

        // SIZE FindMonochromeBlockLocal( const EasyBitMap& MonochromeMap, const POINT prelim, const POINT sentinel )
        // {
        //     if( prelim.x >= sentinel.x || prelim.y >= sentinel.y ) return Dimension;

        //     auto Match = MonochromeMap.Pixels[0];
        //     auto w = MonochromeMap.Dimension.cx;
        //     auto h = MonochromeMap.Dimension.cy;

        //     auto BoundLeft = prelim.x;
        //     for( const auto limit = prelim.x - w + 1; BoundLeft-- > limit && GetColour( BoundLeft, prelim.y ) == Match; )
        //     {}

        //     auto BoundTop = prelim.y;
        //     for( const auto limit = prelim.y - h + 1; BoundTop-- > limit && GetColour( prelim.x, BoundTop ) == Match; )
        //     {}

        //     auto BoundRight = prelim.x;
        //     for( const auto limit = sentinel.x - 1; BoundRight++ < limit && GetColour( BoundRight, prelim.y ) == Match; )
        //     {}

        //     auto BoundBottom = prelim.y;
        //     for( const auto limit = sentinel.y - 1; BoundBottom++ < limit && GetColour( prelim.x, BoundBottom ) == Match; )
        //     {}

        //     POINT new_prelim = { BoundLeft + w, BoundTop + h };
        //     POINT new_sentinel = { BoundRight, BoundBottom };

        //     if( new_prelim != prelim )
        //         return FindMonochromeBlockLocal( MonochromeMap, new_prelim, new_sentinel );  // search in shrinked region
        //     else
        //     {
        //         for( auto y = prelim.y; y-- > BoundTop + 1; )
        //             for( auto x = prelim.x; x-- > BoundLeft + 1; )
        //                 if( GetColour( x, y ) != Match )
        //                 {
        //                     // region #1: { prelim.x(), y+h }, new_sentinel ; everything below y
        //                     auto LocalSearch = FindMonochromeBlockLocal( MonochromeMap, { prelim.x, y + h }, new_sentinel );
        //                     if( LocalSearch != Dimension ) return LocalSearch;

        //                     // region #2: { x+w, prelim.y() }, { new_sentinel.x(), y+h-1 } ; to the right, exclude region #1
        //                     // overlap
        //                     return FindMonochromeBlockLocal( MonochromeMap, { x + w, prelim.y },
        //                                                      { new_sentinel.x, y + h } );
        //                 }
        //         return { BoundLeft + 1, BoundTop + 1 };  // found
        //     }
        // }

        // SIZE FindMonochromeBlock( const EasyBitMap& MonochromeMap )
        // // special use of ControlBitMap format, only use Dimension field & single element Pixels vector
        // // eg. { {0,0},{102,140},{0x0F7F7F7} }
        // {
        //     auto Match = MonochromeMap.Pixels[0];
        //     auto w = MonochromeMap.Dimension.cx;
        //     auto h = MonochromeMap.Dimension.cy;

        //     // disect entire region into sub regions
        //     // each preliminary check point separate w-1/h-1 apart => x_n + w = x_n+1
        //     for( auto prelim_y = h - 1; prelim_y < Dimension.cy; prelim_y += h )
        //         for( auto prelim_x = w - 1; prelim_x < Dimension.cx; prelim_x += w )
        //         {
        //             if( GetColour( prelim_x, prelim_y ) != Match ) continue;
        //             // main search starts here
        //             auto LocalSearch =
        //                 FindMonochromeBlockLocal( MonochromeMap, { prelim_x, prelim_y },
        //                                           { prelim_x + w < Dimension.cx ? prelim_x + w : Dimension.cx,
        //                                             prelim_y + h < Dimension.cy ? prelim_y + h : Dimension.cy } );
        //             if( LocalSearch != Dimension ) return LocalSearch;
        //         }

        //     return Dimension;  // indicate not found
        // }

        void DisplayAt( HWND CanvasHandle, int OffsetX = 0, int OffsetY = 0 )
        {
            if( CanvasHandle == NULL ) return;
            HDC hdc = GetDC( CanvasHandle );
            SetDIBitsToDevice( hdc, OffsetX, OffsetY, Dimension.cx, Dimension.cy,  //
                               0, 0, 0, Dimension.cy, Pixels.data(), &BitMapInformation, DIB_RGB_COLORS );
            ReleaseDC( CanvasHandle, hdc );
        }

        void DisplayAt( int OffsetX, int OffsetY ) { DisplayAt( GetConsoleWindow(), OffsetX, OffsetY ); }

        std::string Code()
        {
            std::size_t x = Origin.x, y = Origin.y, w = Dimension.cx, h = Dimension.cy;
            std::ostringstream OSS;
            OSS << "{";
            OSS << "\n    {" << x << "," << y << "}, {" << w << "," << h << "},";
            OSS << "\n    {";
            for( auto pos = 0uz, y_ = 0uz; y_ < h; ++y_ )
            {
                for( auto x_ = 0uz; x_ < w; ++x_, ++pos )
                {
                    if( x_ % w == 0 ) OSS << "\n    ";

                    if( Pixels[pos].rgbReserved == IGNORE_PIXEL )
                        OSS << IGNORE_TEXT;
                    else
                        OSS << "0x" << std::uppercase << std::setfill( '0' ) << std::setw( 7 ) << std::hex
                            << RGBQUAD_To_Int( Pixels[pos] );

                    if( pos < w * h - 1 ) OSS << ",";
                }
            }
            OSS << "\n    }";
            OSS << "\n};\n";

            return OSS.str();
        }
    };

    //#define AcquireFocus(_Handle_) AcquireFocus_ TemporaryFocus(_Handle_); auto dummy:{0}
    // #define AcquireFocus( _Handle_ )                         \
//     struct                                               \
//     {                                                    \
//         const AcquireFocus_& _Dummy_;                    \
//         bool                 _InLoop_;                   \
//     } TemporaryFocus{ AcquireFocus_{ _Handle_ }, true }; \
//     TemporaryFocus._InLoop_;                             \
//     TemporaryFocus._InLoop_ = false

    // struct AcquireFocus_
    // {
    //     HWND LastForegroundWindow;

    //     inline void BringToForeground( HWND Handle )
    //     {
    //         SendMessage( Handle, WM_ACTIVATE, WA_CLICKACTIVE, 0 );
    //         SetForegroundWindow( Handle );
    //         // SwitchToThisWindow(Handle,true);
    //         // Sleep(100);
    //     }

    //     AcquireFocus_( HWND Handle )
    //     {
    //         LastForegroundWindow = GetForegroundWindow();
    //         BringToForeground( Handle );
    //     }

    //     ~AcquireFocus_() { BringToForeground( LastForegroundWindow ); }
    // };

    struct VirtualDeviceContext
    {
        HWND Handle;
        HDC Destination{}, Source;
        HBITMAP BitMap{};

        VirtualDeviceContext( HWND Handle_, SIZE Dimension ) : Handle{ Handle_ }
        {
            Source = GetDC( Handle );
            if( Dimension == SIZE{ 0, 0 } ) return;
            Destination = CreateCompatibleDC( Source );
            BitMap = CreateCompatibleBitmap( Source, Dimension.cx, Dimension.cy );
            SelectObject( Destination, BitMap );
        }

        ~VirtualDeviceContext()
        {
            if( BitMap ) DeleteObject( BitMap );
            if( Destination ) DeleteDC( Destination );
            ReleaseDC( Handle, Source );
        }

        operator HDC() const noexcept { return Source; }
    };

    struct EasyControl
    {
        HWND Handle;
        DWORD ClickDuration{ 30 };
        DWORD ClickDelay{ 120 };
        DWORD KeyDuration{ 20 };
        DWORD KeyDelay{ 80 };

        EasyControl() : Handle( NULL ) {}

        EasyControl( HWND _hwnd ) : Handle( _hwnd ) {}

        EasyControl( const EasyControl& _Control ) = default;

        EasyControl( std::string_view WindowName, std::string_view ControlName )
        {
            auto WindowHandles = GetWindowHandleByName( WindowName );

#ifdef SHOWNAME
            for( auto& W : WindowHandles )
            {
                ShowHandleName( W );
                ShowAllChild( W );
                std::cout << '\n';
            }
#endif

            switch( WindowHandles.size() )
            {
                case 0 : Handle = NULL; return;
                case 1 :
                    Handle = FindWindowEx( WindowHandles[0], NULL, ControlName.data(), NULL );
                    if( Handle == NULL ) Handle = WindowHandles[0];
                    break;
                default :
                {
                    for( std::size_t i = 0; i < WindowHandles.size(); ++i )
                    {
                        std::cout << "[" << i << "] ";
                        ShowHandleName( WindowHandles[i] );
                    }
                    std::cout << "Pick handle: ";
                    int choice;
                    std::cin >> choice;
                    std::cout << "Chosen: ";
                    ShowHandleName( WindowHandles[choice] );
                    Handle = FindWindowEx( WindowHandles[choice], NULL, ControlName.data(), NULL );
                    break;
                }
            }
        }

        operator HWND() const { return Handle; }

        auto empty() const noexcept { return Handle == NULL; }

        inline void SendKey( WPARAM VirtualKey, LPARAM HardwareScanCode = 1 )
        {
            SendMessage( Handle, WM_KEYDOWN, VirtualKey, HardwareScanCode );
            Sleep( KeyDuration );
            SendMessage( Handle, WM_KEYUP, VirtualKey, HardwareScanCode );
            Sleep( KeyDelay );
        }

        inline void SendChar( WPARAM CharacterCode )
        {
            SendMessage( Handle, WM_CHAR, CharacterCode, 0 );
            Sleep( KeyDelay );
        }

        inline void SendText( const char* Text )
        {
            SendMessage( Handle, WM_SETTEXT, 0, reinterpret_cast<LPARAM>( Text ) );
        }

        SIZE GetClientDimension()
        {
            RECT ClientRect;
            GetClientRect( Handle, &ClientRect );
            return { ClientRect.right, ClientRect.bottom };
        }

        void TranslatePOINT( POINT& Origin )
        {
            static const auto ClientWidth = GetClientDimension().cx;
            static const auto ClientHeight = GetClientDimension().cy;
            if( Origin.x >= 0 && Origin.y >= 0 ) return;

            Origin = POINT{ Origin.x + ( Origin.x < 0 ) * ClientWidth, Origin.y + ( Origin.y < 0 ) * ClientHeight };
        }

        POINT TranslatePOINT( POINT&& Origin )
        {
            TranslatePOINT( Origin );
            return Origin;
        }

        void SendMouseInput( double PercentX, double PercentY )
        {
            static const auto ScreenWidth = GetSystemMetrics( SM_CXSCREEN );
            static const auto ScreenHeight = GetSystemMetrics( SM_CYSCREEN );
            static const auto ClientWidth = GetClientDimension().cx;
            static const auto ClientHeight = GetClientDimension().cy;
            // thread safe initialisation may cause performance issue
            // need modification

            auto ClientPoint = POINT{ .x = long( PercentX * ClientWidth ), .y = long( PercentY * ClientHeight ) };
            if( ClientToScreen( Handle, &ClientPoint ) )
            {
                mouse_event( MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_MOVE | MOUSEEVENTF_LEFTUP,
                             ClientPoint.x * 65535 / ScreenWidth, ClientPoint.y * 65535 / ScreenHeight, 0, 0 );
                Sleep( 100 );

                SendMessage( Handle, WM_RBUTTONDOWN, 0, 0 );
                Sleep( ClickDuration );
                SendMessage( Handle, WM_RBUTTONUP, 0, 0 );
                Sleep( ClickDelay );

                // Click(0);
            }
            else
                std::cout << "\n"
                          << "SendMouseInput to ( 0." << PercentX << " full width, 0." << PercentY
                          << " full height ) failed.\n";
        }

        inline auto MouseLeftEvent( UINT Msg, POINT Point ) const
        {
            return SendMessage( Handle, Msg, MK_LBUTTON, MAKELPARAM( Point.x, Point.y ) );
        }
        inline auto MouseLeftDown( POINT Point ) const { return MouseLeftEvent( WM_LBUTTONDOWN, Point ); }
        inline auto MouseLeftMove( POINT Point ) const { return MouseLeftEvent( WM_MOUSEMOVE, Point ); }
        inline auto MouseLeftUp( POINT Point ) const { return MouseLeftEvent( WM_LBUTTONUP, Point ); }

        inline void Click( POINT Point, unsigned Repeat = 1 ) const
        {
#ifdef SHOWACTION
            std::cout << "[ Click (" << Point.x << ", " << Point.y << ") ]" << std::endl;
#endif
            while( Repeat-- )
            {
                MouseLeftDown( Point );
                Sleep( ClickDuration );
                MouseLeftUp( Point );
                Sleep( ClickDelay );
            }
        }
        inline void Click( int x, int y, unsigned Repeat = 1 ) const { Click( POINT{ x, y }, Repeat ); }

        inline void Drag( POINT POINT_1, POINT POINT_2, bool SmoothDrag = true ) const
        {
#ifdef SHOWACTION
            std::cout << "[ Drag (" << POINT_1.x << ", " << POINT_1.y << ")"
                      << "    To (" << POINT_2.x << ", " << POINT_2.y << ") ]" << std::endl;
#endif

            MouseLeftDown( POINT_1 );
            Sleep( ClickDelay );

            if( SmoothDrag )
            {
                constexpr auto Displacement = 8;

                auto IntervalCount = std::max( std::abs( ( POINT_2.x - POINT_1.x ) / Displacement ),  //
                                               std::abs( ( POINT_2.y - POINT_1.y ) / Displacement ) );
                auto Interval = SIZE{ ( POINT_2.x - POINT_1.x ) / IntervalCount,  //
                                      ( POINT_2.y - POINT_1.y ) / IntervalCount };

                auto CurrentPoint = POINT_1;
                for( auto i = 0; i < IntervalCount; ++i )
                {
                    MouseLeftMove( CurrentPoint );
                    CurrentPoint += Interval;
#ifdef SHOWACTION
                    std::cout << "    To (" << CurrentPoint.x << ", " << CurrentPoint.y << ")" << std::endl;
#endif
                    Sleep( 10 );
                }
            }
            MouseLeftMove( POINT_2 );
            Sleep( ClickDelay );
            MouseLeftUp( POINT_2 );
        }

        EasyBitMap CaptureRegion( POINT Origin, SIZE Dimension ) const
        {
            auto BitMap = EasyBitMap( Origin, Dimension );

            if( Dimension == SIZE{ 0, 0 } ) return BitMap;

            auto VirtualDC = VirtualDeviceContext( Handle, Dimension );

            auto [x, y] = Origin;
            auto [w, h] = Dimension;
            BitBlt( VirtualDC.Destination, 0, 0, w, h, VirtualDC.Source, x, y, SRCCOPY );

            GetDIBits( VirtualDC.Destination, VirtualDC.BitMap, 0, h,  //
                       BitMap.Pixels.data(), &BitMap.BitMapInformation, DIB_RGB_COLORS );

            return BitMap;
        }

        inline bool MatchBitMap( const EasyBitMap& ReferenceBitMap, int SimilarityThreshold = SIMILARITY_THRESHOLD )
        {
            // need complete re-work
            // capture whole image than compare
            (void)SimilarityThreshold;

            auto VirtualDC = VirtualDeviceContext( Handle, ReferenceBitMap.Dimension );
            auto [x, y] = ReferenceBitMap.Origin;
            auto [w, h] = ReferenceBitMap.Dimension;
            BitBlt( VirtualDC.Destination, 0, 0, w, h, VirtualDC.Source, x, y, SRCCOPY );

            for( auto pos{ 0 }, j{ 0 }; j < h; ++j )
                for( auto i{ 0 }; i < w; ++i, ++pos )
                {
                    if( ReferenceBitMap.Pixels[pos].rgbReserved == IGNORE_PIXEL ) continue;

                    // if ( ReferenceBitMap.Pixels[pos] &  REJECT_COLOUR )
                    // {
                    // if ( !Similar( GetPixel(VirtualDC, i, j), ReferenceBitMap.Pixels[pos], SimilarityThreshold ) )
                    // continue; return false;
                    // }
                    // if ( !Similar( GetPixel(VirtualDC, i, j), ReferenceBitMap.Pixels[pos], SimilarityThreshold ) )
                    // return false;

                    //if( ( RGBQUAD_To_Int( REJECT_COLOUR & ReferenceBitMap.Pixels[pos] ) != 0 ) ==
                    //   Similar( GetPixel( VirtualDC, i, j ), ReferenceBitMap.Pixels[pos], SimilarityThreshold ) )
                    return false;
                }
            return true;
        }

        inline RGBQUAD GetColour( POINT Point ) { return CaptureRegion( Point, { 1, 1 } ).Pixels[0]; }

        inline RGBQUAD GetColour( LONG x, LONG y ) { return GetColour( { x, y } ); }

        void Run( void ( *Proc )( EasyControl& ) ) { Proc( *this ); }
        //#define Run(Proc) Run( (void (*)(CreateControl&)) Proc )
    };

}  // namespace EW

int WINAPI WinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow )
{
    EW::EntryPointParamPack = { hInstance, hPrevInstance, lpCmdLine, nCmdShow };
    return EW::Main();
}
#endif
