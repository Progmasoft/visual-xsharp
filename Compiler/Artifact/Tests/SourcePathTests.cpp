// SPDX-FileCopyrightText: 2026 Progmasoft <support@progmasoft.com>
// SPDX-License-Identifier: MPL-2.0 WITH AdditionRef-Progmasoft-Exception-1.1

#include <Progmasoft/Catch3/Assertions.hpp>
#include <string>
#include <string_view>
#include <vector>

#include "Compiler/Artifact/SourcePath.hpp"

namespace
{
    using Visual::XSharp::Artifact::IsNormalizedSourcePath;

    struct PathCase final
    {
        std::u32string_view path;
        bool expected{};
        std::string_view reason;
    };
} // namespace

TEST_CASE("source identity accepts canonical portable project-relative paths")
{
    const std::vector<PathCase> cases{
        { U"Main.vxs", true, "one source segment" },
        { U"Sources/Main.vxs", true, "slash-separated source root" },
        { U"Sources/Nested/Helper.vxs", true, "nested source unit" },
        { U"Türkçe/Örnek.vxs", true, "Unicode source segments" },
        { U"Generated/🚀.vxs", true, "non-BMP Unicode scalar" },
        { U"folder with spaces/source file.vxs",
          true,
          "portable spaces in a path segment" },
    };

    for (const auto &test : cases)
    {
        CAPTURE(test.reason);
        CHECK(IsNormalizedSourcePath(test.path) == test.expected);
    }
}

TEST_CASE("source identity rejects roots aliases and malformed segments")
{
    const std::vector<PathCase> cases{
        { U"", false, "empty path" },
        { U"/Sources/Main.vxs", false, "POSIX rooted path" },
        { U"\\Sources\\Main.vxs", false, "Windows rooted path" },
        { U"C:/Sources/Main.vxs", false, "drive-rooted path" },
        { U"C:Main.vxs", false, "drive-relative path" },
        { U"../Main.vxs", false, "parent traversal" },
        { U"Sources/../Main.vxs", false, "interior parent traversal" },
        { U"./Main.vxs", false, "current-directory alias" },
        { U"Sources/./Main.vxs", false, "interior current-directory alias" },
        { U"Sources//Main.vxs", false, "empty interior segment" },
        { U"Sources/Main.vxs/", false, "trailing separator" },
        { U"Sources/\\Main.vxs", false, "mixed path separators" },
        { U"Sources/Main.VXS", false, "case-changed extension" },
        { U"Sources/Main.cpp", false, "non-source extension" },
        { U"Sources/.vxs", false, "empty basename" },
        { U"Sources/..vxs", true, "dot-leading but non-alias filename" },
        { U"Sources/a:b.vxs", false, "drive separator in a segment" },
    };

    for (const auto &test : cases)
    {
        CAPTURE(test.reason);
        CHECK(IsNormalizedSourcePath(test.path) == test.expected);
    }
}

TEST_CASE("source identity rejects invalid Unicode scalar values")
{
    std::u32string surrogate = U"Sources/";
    surrogate.push_back(static_cast<char32_t>(0xd800U));
    surrogate += U".vxs";

    std::u32string beyondUnicode = U"Sources/";
    beyondUnicode.push_back(static_cast<char32_t>(0x110000U));
    beyondUnicode += U".vxs";

    std::u32string embeddedNull = U"Sources/";
    embeddedNull.push_back(U'\0');
    embeddedNull += U"Main.vxs";

    CHECK_FALSE(IsNormalizedSourcePath(surrogate));
    CHECK_FALSE(IsNormalizedSourcePath(beyondUnicode));
    CHECK_FALSE(IsNormalizedSourcePath(embeddedNull));
}

TEST_CASE("source path validation does not normalize malformed input")
{
    // A verifier rejects noncanonical identities rather than silently fixing
    // them. Otherwise two different artifact bytes could name one source.
    CHECK_FALSE(IsNormalizedSourcePath(U"Sources\\Main.vxs"));
    CHECK_FALSE(IsNormalizedSourcePath(U"Sources//Main.vxs"));
    CHECK_FALSE(IsNormalizedSourcePath(U"Sources/../Main.vxs"));
    CHECK(IsNormalizedSourcePath(U"Sources/Main.vxs"));
}
