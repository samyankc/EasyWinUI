#include <EasyTest.h>
#include <EasyStringExpected.h>
#include <format>

using namespace boost::ut;
using namespace boost::ut::literals;

using namespace EasyString;

constexpr auto StrViewStrongEquality = []( ExStrView LHS, ExStrView RHS ) {
    return LHS == RHS && LHS->begin() == RHS->begin() && LHS->end() == RHS->end();
};

constexpr auto PrepareSearchData = []( ExStrView Text, ExStrView Input ) {
    return std::tuple( Text, Input, Search( Text ).In( Input ), Input | Search( Text ) );
};
auto TestSearch()
{
    "Search bcd In abcde [common case]"_test = [] {
        auto [Text, Input, Result, PipeResult] = PrepareSearchData( "bcd", "abcde" );
        expect( StrViewStrongEquality( Result, PipeResult ) );
        expect( Result.has_value() );
        expect( Result == Text );
        expect( Result->begin() == Input->begin() + 1 );
    };

    "Search empty string in abcde [empty text case]"_test = [] {
        auto [Text, Input, Result, PipeResult] = PrepareSearchData( "", "abcde" );
        expect( Result == PipeResult );
        expect( ! Result.has_value() );
        expect( Result.error() == UnexpectedCondition::EmptyTarget );
    };

    "Search bcd in empty string [empty input case]"_test = [] {
        auto [Text, Input, Result, PipeResult] = PrepareSearchData( "bcd", "" );
        expect( Result == PipeResult );
        expect( ! Result.has_value() );
        expect( Result.error() == UnexpectedCondition::EmptyInput );
    };

    "Search empty string in empty string [empty input & text case]"_test = [] {
        auto [Text, Input, Result, PipeResult] = PrepareSearchData( "", "" );
        expect( Result == PipeResult );
        expect( ! Result.has_value() );
        expect( Result.error() == UnexpectedCondition::EmptyInput ||
                Result.error() == UnexpectedCondition::EmptyTarget );
    };

    "Search fgh in abcde [not found case]"_test = [] {
        auto [Text, Input, Result, PipeResult] = PrepareSearchData( "fgh", "abcde" );
        expect( Result == PipeResult );
        expect( ! Result.has_value() );
        expect( Result.error() == UnexpectedCondition::NotFound );
    };
}

constexpr auto PrepareTrimAnyOfData = []( ExStrView Text, ExStrView Input ) {
    return std::tuple( Text, Input, TrimAnyOf( Text ).From( Input ), Input | TrimAnyOf( Text ) );
};
auto TestTrimAnyOf()
{
    "TrimAnyOf ab from abcdeb is cde"_test = [] {
        auto [Text, Input, Result, PipeResult] = PrepareTrimAnyOfData( "ab", "abcdeb" );
        expect( Result == PipeResult );
        expect( Result.has_value() );
        expect( Result == "cde" );
    };

    "TrimAnyOf empty string from abcdeb is original input"_test = [] {
        auto [Text, Input, Result, PipeResult] = PrepareTrimAnyOfData( "", "abcdeb" );
        expect( StrViewStrongEquality( Result, PipeResult ) );
        expect( Result.has_value() );
        expect( StrViewStrongEquality( Result.value(), Input.value() ) );
    };

    "TrimAnyOf abc from empty string report EmptyInput"_test = [] {
        auto [Text, Input, Result, PipeResult] = PrepareTrimAnyOfData( "abc", "" );
        expect( Result == PipeResult );
        expect( ! Result.has_value() );
        expect( Result.error() == UnexpectedCondition::EmptyInput );
    };

    "TrimAnyOf abc from abcbca report ReachingEnd"_test = [] {
        auto [Text, Input, Result, PipeResult] = PrepareTrimAnyOfData( "abc", "abcbca" );
        expect( Result == PipeResult );
        expect( ! Result.has_value() );
        expect( Result.error() == UnexpectedCondition::ReachingEnd );
    };
}

constexpr auto PrepareBeforeData = []( ExStrView Text, ExStrView Input ) {
    return std::tuple( Text, Input, Before( Text )( Input ), Input | Before( Text ) );
};
auto TestBefore()
{
    "abcdef | Before( cde ) is ab"_test = [] {
        auto [Text, Input, Result, PipeResult] = PrepareBeforeData( "cde", "abcdef" );
        expect( Result.has_value() );
        expect( StrViewStrongEquality( Result, PipeResult ) );
        expect( Result == "ab" );
        expect( Result->begin() == Input->begin() );
    };

    "abcdef | Before( ab ) report BeforeBegin"_test = [] {
        auto [Text, Input, Result, PipeResult] = PrepareBeforeData( "ab", "abcdef" );
        expect( Result == PipeResult );
        expect( ! Result.has_value() );
        expect( Result.error() == UnexpectedCondition::BeforeBegin );
    };

    "abcdef | Before( gh ) report NotFound"_test = [] {
        auto [Text, Input, Result, PipeResult] = PrepareBeforeData( "gh", "abcdef" );
        expect( Result == PipeResult );
        expect( ! Result.has_value() );
        expect( Result.error() == UnexpectedCondition::NotFound );
    };

    "abcdef | Before( empty ) report EmptyTarget"_test = [] {
        auto [Text, Input, Result, PipeResult] = PrepareBeforeData( "", "abcdef" );
        expect( Result == PipeResult );
        expect( ! Result.has_value() );
        expect( Result.error() == UnexpectedCondition::EmptyTarget );
    };

    "empty | Before( abc ) report EmptyInput"_test = [] {
        auto [Text, Input, Result, PipeResult] = PrepareBeforeData( "abc", "" );
        expect( Result == PipeResult );
        expect( ! Result.has_value() );
        expect( Result.error() == UnexpectedCondition::EmptyInput );
    };
}

int main()
{
    TestSearch();
    TestTrimAnyOf();
    TestBefore();
    return 0;
}