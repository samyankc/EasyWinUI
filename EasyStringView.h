#ifndef _EASYSTRINGVIEW_H_
#define _EASYSTRINGVIEW_H_

#include <string>
#include <optional>
#include <string_view>

namespace
{
    struct SelectivePtr
    {};
}

struct EasyStringView : std::string_view
{
    bool NullTerminated{ false };

    using std::string_view::basic_string_view;

    constexpr EasyStringView( const char* Source ) : basic_string_view( Source ), NullTerminated{ true } {}

    template<std::size_t N>
    constexpr EasyStringView( const char ( &Source )[N] ) : basic_string_view( Source, N - 1 ), NullTerminated{ true }
    {}

    constexpr EasyStringView( const std::string& Source ) : basic_string_view( Source ), NullTerminated{ true } {}

    constexpr EasyStringView( const EasyStringView& ) = default;
    constexpr EasyStringView( std::string_view Source ) : std::string_view( Source ), NullTerminated{ false } {}

    constexpr auto c_str() const -> std::optional<const char*>
    {
        if( NullTerminated ) return this->data();
        return std::nullopt;
    }
};

#endif
