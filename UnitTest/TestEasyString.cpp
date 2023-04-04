#include <EasyTest.h>
#include <EasyString.h>
#include <format>

using namespace boost::ut;
using namespace boost::ut::literals;

using namespace EasyString;

constexpr auto StrViewStrongEquality = []( StrView LHS, StrView RHS ) {
    return LHS == RHS && LHS.begin() == RHS.begin() && LHS.end() == RHS.end();
};

constexpr auto PrepareSearchData = []( StrView Text, StrView Input ) {
    return std::tuple( Text, Input, Search( Text ).In( Input ), Input | Search( Text ) );
};

auto TestSearch()
{
    "Search bcd In abcde [common case]"_test = [] {
        auto [Text, Input, Result, PipeResult] = PrepareSearchData( "bcd", "abcde" );
        expect( StrViewStrongEquality( Result, PipeResult ) );

        expect( Result == Text );
        expect( Result.begin() == Input.begin() + 1 );
    };

    "Search empty string in abcde [empty text case]"_test = [] {
        auto [Text, Input, Result, PipeResult] = PrepareSearchData( "", "abcde" );
        expect( StrViewStrongEquality( Result, PipeResult ) );

        expect( Result.empty() );
        expect( Result.begin() == Input.begin() );
    };

    "Search bcd in empty string [empty input case]"_test = [] {
        auto [Text, Input, Result, PipeResult] = PrepareSearchData( "bcd", "" );
        expect( StrViewStrongEquality( Result, PipeResult ) );

        expect( Result.empty() );
        expect( Result.begin() == Input.begin() );
    };

    "Search empty string in empty string [empty input & text case]"_test = [] {
        auto [Text, Input, Result, PipeResult] = PrepareSearchData( "", "" );
        expect( StrViewStrongEquality( Result, PipeResult ) );

        expect( Result.empty() );
        expect( Result.begin() == Input.begin() );
    };

    "Search fgh in abcde [not found case]"_test = [] {
        auto [Text, Input, Result, PipeResult] = PrepareSearchData( "fgh", "abcde" );
        expect( StrViewStrongEquality( Result, PipeResult ) );

        expect( Result.empty() );
        expect( Result.begin() == Input.end() );
        expect( Result.end() == Input.end() );
    };
}

constexpr auto PrepareBeforeData = []( StrView Text, StrView Input ) {
    return std::tuple( Text, Input, Before( Text )( Input ), Input | Before( Text ) );
};
auto TestBefore()
{
    "abcdef | Before( cde ) is ab"_test = [] {
        auto [Text, Input, Result, PipeResult] = PrepareBeforeData( "cde", "abcdef" );
        expect( StrViewStrongEquality( Result, PipeResult ) );
        expect( Result == "ab" );
        expect( Result.begin() == Input.begin() );
    };

    "abcdef | Before( ab ) is empty"_test = [] {
        auto [Text, Input, Result, PipeResult] = PrepareBeforeData( "ab", "abcdef" );
        expect( StrViewStrongEquality( Result, PipeResult ) );
        expect( Result.empty() );
        expect( Result.begin() == Input.begin() );
    };

    "abcdef | Before( gh ) is empty"_test = [] {
        auto [Text, Input, Result, PipeResult] = PrepareBeforeData( "gh", "abcdef" );
        expect( StrViewStrongEquality( Result, PipeResult ) );
        expect( Result.empty() );
        expect( Result.begin() == Input.end() );
    };

    "abcdef | Before( empty ) is empty at abcdef begin"_test = [] {
        auto [Text, Input, Result, PipeResult] = PrepareBeforeData( "", "abcdef" );
        expect( StrViewStrongEquality( Result, PipeResult ) );
        expect( Result.empty() );
        expect( Result.begin() == Input.begin() );
    };

    "empty | Before( abc ) is empty"_test = [] {
        auto [Text, Input, Result, PipeResult] = PrepareBeforeData( "abc", "" );
        expect( StrViewStrongEquality( Result, PipeResult ) );
        expect( Result.empty() );
        expect( Result.begin() == Input.begin() );
    };
}

int main()
{
    TestSearch();
    TestBefore();
    return 0;
}