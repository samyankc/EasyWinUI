/*

Usage:
auto Control = CreateControl("Window Class Name","Control Class Name");

Control.Click(x,y);
Control.Drag({x1,y2},{x2,y2});
COLORREF colour = Control.GetColour(x,y);

*/

#ifndef EASYWINCONTROL_H
#define EASYWINCONTROL_H

#include <iostream>
#include <cstring>
#include <vector>
#include <map>
#include <Windows.h>
#include <gdiplus.h>
#include <string>
#include <iomanip>
#include <EasyNotepad.h>

#define SIMILARITY_THRESHOLD  20

#define IGNORE_COLOUR       0x1000000
#define REJECT_COLOUR       0x2000000  // EXCLUDE_COLOUR | 0xBBGGRR  ==>  all colours allowed except 0xBBGGRR ( exact match )

//#define IGNORE_TEXT         "_WwWwWwW_"
#define IGNORE_TEXT         "0x1000000"
#define _WwWwW_             IGNORE_COLOUR
#define _WwWwWwW_           IGNORE_COLOUR

//#define Associate(Object,Method) auto Method = std::bind_front(&decltype(Object)::Method, &Object)
/*
#define Associate(Object,Method)    \
auto Method = [&Object = Object]    \
<typename... Ts>(Ts&&... Args)      \
{ return Object.Method( std::forward<Ts>(Args)...); }
*/

constexpr int nMaxCount = 40;
char lpText[nMaxCount];

void ShowHandleName(HWND hwnd)
{
	unsigned long long _hwnd = (unsigned long long)(hwnd);
	auto digits = [](unsigned long long n){ for(int i=1; i<18; ++i) if ((n/=10)==0) return i; return 0; };
	std::cout<<_hwnd; for (int i = 12 - digits(_hwnd); i-->0;) std::cout<<' ';

	GetWindowText(hwnd,lpText,nMaxCount);	std::cout<<"[ "<<lpText<<" ] ";
	for (int i = 25 - strlen(lpText); i-->0;) std::cout<<' ';
	GetClassName(hwnd,lpText,nMaxCount);	std::cout<<"[ "<<lpText<<" ] ";
	std::cout<<'\n';
}

void ShowAllChild(HWND hwnd)
{
	EnumChildWindows(
		hwnd,
		[]( HWND hcwnd, LPARAM lParam ) -> int
		{
			std::cout<<"\\ ";
			ShowHandleName(hcwnd);
			return true;
		},
		0
	);
}

HWND ObtainFocusHandle(int Delay=2000)
{
	Sleep(Delay);
	HWND ret = GetFocus();
	if( ret == NULL ) ret = GetForegroundWindow();
	return ret;
}

constexpr WPARAM operator "" _VK(char ch)
{
	if ( ch>='a' && ch<='z' ) ch -= 'a'-'A';
	return ch;
}

std::vector<HWND> GetWindowHandleByName(const char* name)
{
	std::vector<HWND> Handles;
    Handles.reserve(4);

	struct DATA{
		std::vector<HWND>& Handles;
		const char* name;
	} lParam{Handles,name};

	EnumWindows(
		[]( HWND hwnd, LPARAM lParam ) -> int
		{
            DATA* lParam_ptr = reinterpret_cast<DATA*>(lParam);
			GetClassName(hwnd,lpText,nMaxCount);
			if ( strstr(lpText,lParam_ptr->name) != NULL )
				lParam_ptr->Handles.push_back(hwnd);
			return true;
		},
		reinterpret_cast<LPARAM>(&lParam)
	);

	return Handles;
}

bool Similar(int a, int b, int SimilarityThreshold = SIMILARITY_THRESHOLD)
{
	auto Delta = [](int x, int y){ return (x>y)? (x-y):(y-x); };
	return	Delta( GetRValue(a) , GetRValue(b) )<=SimilarityThreshold &&
			Delta( GetGValue(a) , GetGValue(b) )<=SimilarityThreshold &&
			Delta( GetBValue(a) , GetBValue(b) )<=SimilarityThreshold ;
};

struct Point
{
	LPARAM POINT_DATA;
	constexpr Point(LPARAM _src):POINT_DATA(_src){}
	constexpr Point(int x, int y):POINT_DATA(MAKELPARAM(x,y)){}
	constexpr operator LPARAM&() { return POINT_DATA; }
    constexpr Point operator-() const { return {-x(),-y()}; }
    constexpr Point operator<<=(const Point& _offset) const { return { x() + _offset.x(), y() + _offset.y() }; }
    constexpr Point& operator+=(const Point& _offset) { return *this = { x() + _offset.x(), y() + _offset.y() }; }
	constexpr int16_t x() const { return LOWORD(POINT_DATA); }
	constexpr int16_t y() const { return HIWORD(POINT_DATA); }
    constexpr friend bool operator==(const Point& lhs, const Point& rhs){ return lhs.POINT_DATA == rhs.POINT_DATA; }
};
using Offset = Point;

struct ControlBitMap
{
    Point Origin;
    Offset Dimension;
	std::vector<COLORREF> Pixels;

	ControlBitMap IsolateColour(COLORREF KeepColour, COLORREF ColourMask = IGNORE_COLOUR, int SimilarityThreshold = SIMILARITY_THRESHOLD)
	{
        ControlBitMap NewMap{ *this };
		for ( COLORREF& CurrentPixel : NewMap.Pixels )
			if ( !Similar( CurrentPixel, KeepColour, SimilarityThreshold ) )
                CurrentPixel = KeepColour | ColourMask;
            //else CurrentPixel = KeepColour;
        return NewMap;
	}

    COLORREF GetColour(int x, int y)
    {
        return Pixels[ x + y*Dimension.x() ];
    }

    COLORREF& operator[](const Point& CheckPoint)
    {
        return Pixels[ CheckPoint.x() + CheckPoint.y()*Dimension.x() ];
    }

    Point FindMonochromeBlockLocal(const ControlBitMap& MonochromeMap,const Point prelim, const Point sentinel)
    {
        if ( prelim.x() >= sentinel.x() || prelim.y() >= sentinel.y() ) return Dimension;

        auto Match = MonochromeMap.Pixels[0];
        auto w = MonochromeMap.Dimension.x();
        auto h = MonochromeMap.Dimension.y();

        auto BoundLeft = prelim.x();
        for ( const auto limit = prelim.x() - w + 1;
              BoundLeft --> limit &&
              GetColour(BoundLeft, prelim.y()) == Match; );

        auto BoundTop = prelim.y();
        for ( const auto limit = prelim.y() - h + 1;
              BoundTop --> limit &&
              GetColour(prelim.x(), BoundTop) == Match; );

        auto BoundRight = prelim.x();
        for ( const auto limit = sentinel.x() - 1;
              BoundRight ++< limit &&
              GetColour(BoundRight, prelim.y()) == Match; );

        auto BoundBottom = prelim.y();
        for ( const auto limit = sentinel.y() - 1;
              BoundBottom ++< limit &&
              GetColour(prelim.x(), BoundBottom) == Match; );

        Point new_prelim = { BoundLeft+w, BoundTop+h };
        Point new_sentinel = { BoundRight, BoundBottom };

        if ( new_prelim != prelim )
            return FindMonochromeBlockLocal(MonochromeMap,new_prelim,new_sentinel); // search in shrinked region
        else
        {
            for ( auto y = prelim.y(); y-->BoundTop+1;)
                for ( auto x = prelim.x(); x-->BoundLeft+1; )
                    if ( GetColour(x, y) != Match )
                    {
                        // region #1: { prelim.x(), y+h }, new_sentinel ; everything below y
                        auto LocalSearch = FindMonochromeBlockLocal( MonochromeMap, { prelim.x(), y+h }, new_sentinel );
                        if ( LocalSearch != Dimension ) return LocalSearch;

                        // region #2: { x+w, prelim.y() }, { new_sentinel.x(), y+h-1 } ; to the right, exclude region #1 overlap
                        return FindMonochromeBlockLocal( MonochromeMap, { x+w, prelim.y() }, { new_sentinel.x(), y+h } );
                    }
            return { BoundLeft+1, BoundTop+1 }; // found
        }
    }


    Point FindMonochromeBlock(const ControlBitMap& MonochromeMap)
    // special use of ControlBitMap format, only use Dimension field & single element Pixels vector
    // eg. { {0,0},{102,140},{0x0F7F7F7} }
    {
        auto Match = MonochromeMap.Pixels[0];
        auto w = MonochromeMap.Dimension.x();
        auto h = MonochromeMap.Dimension.y();

        // disect entire region into sub regions
        // each preliminary check point separate w-1/h-1 apart => x_n + w = x_n+1
        for (auto prelim_y=h-1; prelim_y < Dimension.y(); prelim_y += h )
            for (auto prelim_x=w-1; prelim_x < Dimension.x(); prelim_x += w )
            {
                if ( GetColour(prelim_x,prelim_y) != Match ) continue;
                // main search starts here
                auto LocalSearch = FindMonochromeBlockLocal(
                                        MonochromeMap,
                                        { prelim_x, prelim_y },
                                        { prelim_x+w < Dimension.x() ? prelim_x+w : Dimension.x(),
                                          prelim_y+h < Dimension.y() ? prelim_y+h : Dimension.y() } );
                if ( LocalSearch != Dimension ) return LocalSearch;
            }

        return Dimension; // indicate not found
    }

	void DisplayAt(HWND hSTDOUT, int OffsetX, int OffsetY)
	{
		#define BORDER_COLOUR	0xFF
		if (hSTDOUT==NULL) hSTDOUT = GetConsoleWindow();
		HDC hdcSTDOUT = GetDC(hSTDOUT);

		for (int i = 0; i < Dimension.x(); ++i) SetPixel( hdcSTDOUT, OffsetX+i, OffsetY-1, BORDER_COLOUR);
		for (int j = 0, pos = 0; j < Dimension.y(); ++j)
		{
			SetPixel( hdcSTDOUT, OffsetX-1, OffsetY+j, BORDER_COLOUR);
			for (int i = 0; i < Dimension.x(); ++i)
				SetPixel( hdcSTDOUT, OffsetX+i, OffsetY+j, Pixels[pos++]);
			SetPixel( hdcSTDOUT, OffsetX-1+Dimension.x(), OffsetY+j, BORDER_COLOUR);
		}
		for (int i = 0; i < Dimension.x(); ++i) SetPixel( hdcSTDOUT, OffsetX+i, OffsetY-1+Dimension.y(), BORDER_COLOUR);

		ReleaseDC(hSTDOUT,hdcSTDOUT);
	}

	void DisplayAt(int OffsetX, int OffsetY)
	{
		DisplayAt(GetConsoleWindow(),OffsetX,OffsetY);
	}

    std::string Code()
    {
        std::ostringstream OSS;
        OSS<<"{";
        OSS<<"\n    {"<< Origin.x() <<","<<Origin.y()<<"}, {"<< Dimension.x() <<","<< Dimension.y() <<"},";
        OSS<<"\n    {";
        for (int pos = 0, y = 0; y < Dimension.y(); ++y)
        {
            for (int x = 0; x < Dimension.x(); ++x, ++pos)
            {
                if ( x % Dimension.x() == 0 ) OSS<<"\n    ";

                if ( Pixels[pos] & IGNORE_COLOUR )
                    OSS << IGNORE_TEXT;
                else
                    OSS << "0x" << std::uppercase << std::setfill('0') << std::setw(7) << std::hex << Pixels[pos];

                if ( pos < Dimension.x() * Dimension.y() - 1 ) OSS<<" , ";
            }
        }
        OSS<<"\n    }";
        OSS<<"\n};\n";

        return OSS.str();
    }

};

//#define AcquireFocus(_Handle_) AcquireFocus_ TemporaryFocus(_Handle_); auto dummy:{0}
#define AcquireFocus(_Handle_) \
	struct{ const AcquireFocus_& _Dummy_; bool _InLoop_; } \
	TemporaryFocus{ AcquireFocus_{_Handle_},true }; \
	TemporaryFocus._InLoop_; \
	TemporaryFocus._InLoop_ = false

struct AcquireFocus_
{
	HWND LastForegroundWindow;

	inline void BringToForeground(HWND Handle)
	{
		SendMessage(Handle,WM_ACTIVATE,WA_CLICKACTIVE,0);
		SetForegroundWindow(Handle);
        // SwitchToThisWindow(Handle,true);
        // Sleep(100);
	}

	AcquireFocus_(HWND Handle)
	{
		LastForegroundWindow = GetForegroundWindow();
		BringToForeground(Handle);
	}

	~AcquireFocus_()
	{
		BringToForeground(LastForegroundWindow);
	}
};

struct VirtualDeviceContext
{
    HDC hdcDestin;
    HBITMAP hBMP;

    VirtualDeviceContext(const HDC& hdcSource, int w, int h)
    {
        hdcDestin = CreateCompatibleDC(hdcSource);
        hBMP = CreateCompatibleBitmap(hdcSource,w,h);
        SelectObject(hdcDestin, hBMP);
    }

    ~VirtualDeviceContext()
    {
		DeleteObject(hBMP);
		DeleteDC(hdcDestin);
    }

    operator HDC() { return hdcDestin; }
};

struct CreateControl
{
	HWND	Handle;
	int		ClickDuration{30};
	int		ClickDelay{120};
	int		KeyDuration{20};
	int		KeyDelay{80};

	CreateControl() : Handle(NULL)
	{}

	CreateControl(HWND _hwnd) : Handle(_hwnd)
	{}

    CreateControl(const CreateControl& _Control) = default;

	CreateControl(const char* WindowName, const char* ControlName)
	{
		std::vector<HWND> WindowHandles = GetWindowHandleByName( WindowName );

		#ifdef SHOWNAME
			for ( auto& W : WindowHandles ) { ShowHandleName(W); ShowAllChild(W); std::cout<<'\n'; }
		#endif

		switch ( WindowHandles.size() )
		{
			case 0:
				Handle = NULL;
				return;
			case 1:
				Handle = FindWindowEx(WindowHandles[0],NULL,ControlName,NULL);
				if (Handle==NULL) Handle = WindowHandles[0];
				break;
			default:
			{
				for (int i=0; i<WindowHandles.size(); ++i)
				{
					std::cout<<"["<<i<<"] ";
					ShowHandleName(WindowHandles[i]);
				}
				std::cout<<"Pick handle: ";
				int choice; std::cin>>choice;
				std::cout<<"Chosen: ";
				ShowHandleName(WindowHandles[choice]);
				Handle = FindWindowEx(WindowHandles[choice],NULL,ControlName,NULL);
				break;
			}
		}
	}

	operator HWND()
	{
		return Handle;
	}

	inline void SendKey(WPARAM VirtualKey, LPARAM HardwareScanCode = 1)
	{
		SendMessage( Handle, WM_KEYDOWN, VirtualKey, HardwareScanCode );	Sleep(KeyDuration);
		SendMessage( Handle, WM_KEYUP, VirtualKey, HardwareScanCode );		Sleep(KeyDelay);
	}

	inline void SendChar(WPARAM CharacterCode)
	{
        SendMessage( Handle, WM_CHAR, CharacterCode, 0 );   Sleep(KeyDelay);
	}

	inline void SendText(const char* Text)
	{
		SendMessage( Handle, WM_SETTEXT, 0, (LPARAM)Text );
	}


    Offset GetClientDimension()
    {
        static const auto ClientDimension
        = [&]{
            RECT ClientRect;
            GetClientRect(Handle,&ClientRect);
            return Offset{ClientRect.right, ClientRect.bottom};
        }();
        return ClientDimension;
    }

    void TranslatePoint(Point& Origin)
    {
        static const auto ClientWidth = GetClientDimension().x();
        static const auto ClientHeight = GetClientDimension().y();
        if ( Origin.x() >= 0 && Origin.y() >= 0 ) return;

        Origin = Point{ Origin.x() + (Origin.x() < 0) * ClientWidth ,
                        Origin.y() + (Origin.y() < 0) * ClientHeight };
    }

    Point TranslatePoint(Point&& Origin)
    {
        TranslatePoint(Origin);
        return Origin;
    }

	void SendMouseInput(double PercentX, double PercentY)
	{
        static const auto ScreenWidth  = GetSystemMetrics(SM_CXSCREEN);
        static const auto ScreenHeight = GetSystemMetrics(SM_CYSCREEN);
        static const auto ClientWidth = GetClientDimension().x();
        static const auto ClientHeight = GetClientDimension().y();
		if ( POINT ClientPoint{long(PercentX*ClientWidth),long(PercentY*ClientHeight)};
             ClientToScreen(Handle,&ClientPoint) )
        {
			mouse_event( MOUSEEVENTF_ABSOLUTE|MOUSEEVENTF_MOVE|MOUSEEVENTF_LEFTUP,
						 ClientPoint.x*65535/ScreenWidth, ClientPoint.y*65535/ScreenHeight, 0, 0 );
			Sleep(100);

            SendMessage( Handle, WM_RBUTTONDOWN, 0, 0 );	Sleep(ClickDuration);
            SendMessage( Handle, WM_RBUTTONUP  , 0, 0 );	Sleep(ClickDelay);

			//Click(0);
		}
		else std::cout<<"\n"<<"SendMouseInput to ( 0."<< PercentX <<" full width, 0."<< PercentY <<" full height ) failed.\n";
	}

	inline void Click(int x, int y, int Repeat = 1)
	{
		Click({x,y},Repeat);
	}

	inline void Click(Point POINT, int Repeat = 1)
    {
		#ifdef SHOWACTION
			std::cout<<"[ Click ("<<POINT.x()<<", "<<POINT.y()<<") ]"<<endl;
		#endif
		for (int i = Repeat; i-->0;)
		{
			SendMessage( Handle, WM_LBUTTONDOWN, MK_LBUTTON, POINT );	Sleep(ClickDuration);
            SendMessage( Handle, WM_LBUTTONUP  , MK_LBUTTON, POINT );	Sleep(ClickDelay);
		}
	}

	inline void Drag(Point POINT_1, Point POINT_2, bool SmoothDrag = true)
	{
		#ifdef SHOWACTION
			std::cout<<"[ Drag ("<<POINT_1.x()<<", "<<POINT_1.y()<<")";
			std::cout<<"    To ("<<POINT_2.x()<<", "<<POINT_2.y()<<") ]"<<endl;
		#endif

		SendMessage( Handle, WM_LBUTTONDOWN, MK_LBUTTON, POINT_1 );		Sleep(ClickDelay);

        if (SmoothDrag)
        {
            constexpr auto Displacement = 8;

            auto IntervalCount = std::max( std::abs( ( POINT_2.x() - POINT_1.x() ) / Displacement ),  //
                                           std::abs( ( POINT_2.y() - POINT_1.y() ) / Displacement ) );
            auto Delta = Offset{ ( POINT_2.x() - POINT_1.x() ) / IntervalCount,  //
                                 ( POINT_2.y() - POINT_1.y() ) / IntervalCount };

            auto CurrentPoint = POINT_1;
            for( auto i = 0; i < IntervalCount; ++i )
            {
                SendMessage( Handle, WM_MOUSEMOVE, MK_LBUTTON, CurrentPoint );
                CurrentPoint += Delta;
                #ifdef SHOWACTION
                std::cout << "    To (" << CurrentPoint.x() << ", " << CurrentPoint.y() << ")" << endl;
                #endif
                Sleep( 10 );
            }
        }
		SendMessage( Handle, WM_MOUSEMOVE  , MK_LBUTTON, POINT_2 );		Sleep(ClickDelay);
		SendMessage( Handle, WM_LBUTTONUP  , MK_LBUTTON, POINT_2 );
	}

	ControlBitMap CaptureRegion(Point Origin, Offset Dimension) const
	{
		int x{Origin.x()}, y{Origin.y()}, w{Dimension.x()}, h{Dimension.y()};

		ControlBitMap BitMap{ Origin, Dimension, {} };
		BitMap.Pixels.reserve(w*h);

		HDC hdcSource = GetDC(Handle);
        auto VirtualDC = VirtualDeviceContext(hdcSource,w,h);

		BitBlt(
            VirtualDC, 0, 0,
            w, h,
            hdcSource, x, y,
            SRCCOPY
        );
		ReleaseDC(Handle, hdcSource);

		for( int j = 0; j < h; ++j)
			for( int i = 0; i < w; ++i)
				BitMap.Pixels.push_back( GetPixel(VirtualDC, i, j) );

		return BitMap;
	}

	inline bool MatchBitMap(const ControlBitMap& ReferenceBitMap, int SimilarityThreshold = SIMILARITY_THRESHOLD)
	{
		int x{ReferenceBitMap.Origin.x()}, y{ReferenceBitMap.Origin.y()}, w{ReferenceBitMap.Dimension.x()}, h{ReferenceBitMap.Dimension.y()};

		HDC hdcSource = GetDC(Handle);
        auto VirtualDC = VirtualDeviceContext(hdcSource,w,h);

        BitBlt(
            VirtualDC, 0, 0,
            w, h,
            hdcSource, x, y,
            SRCCOPY
        );
		ReleaseDC(Handle, hdcSource);

		for( int pos{0},j{0}; j < h; ++j)
			for( int i{0}; i < w; ++i, ++pos)
			{
                if ( ReferenceBitMap.Pixels[pos] == IGNORE_COLOUR ) continue;

                // if ( ReferenceBitMap.Pixels[pos] &  REJECT_COLOUR )
                // {
                    // if ( !Similar( GetPixel(VirtualDC, i, j), ReferenceBitMap.Pixels[pos], SimilarityThreshold ) ) continue;
                    // return false;
                // }
                // if ( !Similar( GetPixel(VirtualDC, i, j), ReferenceBitMap.Pixels[pos], SimilarityThreshold ) )
					// return false;

                if ( ( REJECT_COLOUR & ReferenceBitMap.Pixels[pos] ) != 0
                    == Similar( GetPixel(VirtualDC, i, j), ReferenceBitMap.Pixels[pos], SimilarityThreshold ) )
                    return false;
			}
		return true;
	}

	inline COLORREF GetColour(Point POINT)
	{
		return CaptureRegion(POINT,{1,1}).Pixels[0];
	}

	inline COLORREF GetColour(int x, int y)
	{
		return GetColour({x,y});
	}

	void Run(void (*Proc)(CreateControl&))
	{
		Proc(*this);
	}
    //#define Run(Proc) Run( (void (*)(CreateControl&)) Proc )
};


#endif


