#include <EasyWinUI.h>
#include <EasyWinControl.h>

int EWUI::Main()
{
    auto MainWindow = Window( {}, "Test Window" )  //
                          .Origin( { 10, 10 } )
                          .Dimension( { -1, -1 } );


    auto HeaderLabel = TextLabel()  //
                           .Label( "Header, Click button to change this text" )
                           .Dimension( { 400, 50 } );


    auto ActionButton = Button()  //
                            .Label( "Click Me" )
                            .Dimension( { 200, 100 } )
                            .Action( [&] { HeaderLabel = "Button Clicked."; } );


    auto InputBox1 = TextBox().Dimension({200,24});
    auto InputBox2 = TextBox().Dimension({200,24});

    auto InputArea = TextArea().Dimension( { 200, 100 } );

    auto PopupWindow1 = PopupWindow().Dimension( { 300, 600 } );
    PopupWindow1.Hide();


    PopupWindow1 << TextLabel()                    //
                        .Dimension( { 100, 200 } )  //
                 << Button()                       //
                        .Label( "test" )
                        .Action( [&] {
                            auto MyControl = CreateControl( "LDPlayerMainFrame", "RenderWindow" );
                            auto Pic = MyControl.CaptureRegion( { 20, 20 }, { 100, 200 } );
                            Pic.DisplayAt( PopupWindow1, 1, 1 );
                        } );


    MainWindow << HeaderLabel   //
               << InputBox1     //
               << InputBox2     //
               << ActionButton  //
               << Button()      //
                      .Label( "Click Me Again" )
                      .Dimension( { 200, 120 } )
                      .Action( [&] { HeaderLabel = "Button Clicked Again."; } )

               << Button()  //
                      .Label( "Popup" )
                      .Action( [&] {
                          PopupWindow1.ToggleVisibility();
                          Notepad.exe << InputBox1.Content() << "\n" << InputBox2.Content() << "\n";
                      } );


    MainWindow << PopupWindow1;

    //MonitorHandle = InputBox1;

    return MainWindow;
}