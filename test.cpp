#include "EasyWinUI.hpp"

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