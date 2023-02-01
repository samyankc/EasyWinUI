#include <EasyWin.h>
#include <EasyTest.h>

using namespace EasyMeta;
using namespace boost::ut;

int EW::Main()
{
    auto MainWindow = Window << "This_is_a_test_window" << Dimension( { 200, 100 } );
    return MainWindow;
}

//int main() { std::cout << "hello world\n"; }
