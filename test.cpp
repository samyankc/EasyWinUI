#include <EasyWinUI.h>

int EWUI::Main()
{
    auto MainWindow = Window( {
        .Label = "Test Window",
        .Origin = { 10, 10 },
        .Dimension = { 300, 500 },
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

    auto InputBox1 = TextBox( {} );
    auto InputBox2 = TextBox( {} );

    auto InputArea = TextArea( { .Dimension = { 200, 100 } } );

    auto PopupWindow1 = PopupWindow( {
        .Dimension = { 300, 400 },
    } );


    auto PopupButton1 = Button( {
        .Label = "Popup",
        .Action =
            [&] {

            },
    } );

    MainWindow << HeaderLabel   //
               << InputBox1     //
               << InputBox2     //
               << ActionButton  //
               << Button( {
                      .Label = "Click Me Again",
                      .Dimension = { 200, 120 },
                      .Action = [&] { HeaderLabel = "Button Clicked Again."; },
                  } )
               << PopupWindow1;

    PopupWindow1 << Button( {
        .Label = "test",
        .Action = [&] { InputBox1 = " hi"; },
    } );
    //MonitorHandle = InputBox1;

    return MainWindow;
}