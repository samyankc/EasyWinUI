#include "../index_range.h"
#include <iostream>
#include <format>

using namespace IndexRange;

int main()
{
    for( auto i : Range( 10 ) ) std::cout << i << ',';
    std::cout << '\n';

    for( auto i : Range( 1, 10 ) ) std::cout << i << ',';
    std::cout << '\n';

    for( auto i : Range( 3, 20 ) | Drop( 2 ) | Reverse | Drop( 3 ) | Reverse | Take( 5 ) ) std::cout << i << ',';
    std::cout << '\n';
}