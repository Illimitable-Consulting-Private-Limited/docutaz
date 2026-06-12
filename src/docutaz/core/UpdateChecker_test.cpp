#include "gtest/gtest.h"

#include "docutaz/core/UpdateChecker.h"

using Docutaz::UpdateChecker;

// The update banner fires only when isNewerVersion(latest, installed) is true.
// These tests pin that it is a STRICT greater-than, so a rollback can never
// prompt a "downgrade as update".

TEST(update_checker, prompts_only_for_strictly_newer)
{
    EXPECT_TRUE(UpdateChecker::isNewerVersion("2.0.1", "2.0.0"));
    EXPECT_TRUE(UpdateChecker::isNewerVersion("2.1.0", "2.0.9"));
    EXPECT_TRUE(UpdateChecker::isNewerVersion("3.0.0", "2.9.9"));
    EXPECT_TRUE(UpdateChecker::isNewerVersion("v2.0.1", "2.0.0"));   // tolerates 'v'
}

TEST(update_checker, no_prompt_when_equal)
{
    EXPECT_FALSE(UpdateChecker::isNewerVersion("2.0.0", "2.0.0"));
    EXPECT_FALSE(UpdateChecker::isNewerVersion("v2.0.0", "2.0.0"));
}

TEST(update_checker, no_prompt_on_rollback)
{
    // GitHub's "latest" is older than what's installed -> must NOT prompt.
    EXPECT_FALSE(UpdateChecker::isNewerVersion("2.0.0", "2.1.0"));
    EXPECT_FALSE(UpdateChecker::isNewerVersion("v1.9.9", "2.0.0"));
    EXPECT_FALSE(UpdateChecker::isNewerVersion("1.0.0", "2.0.0"));
}

TEST(update_checker, suffixes_and_unparseable_never_downgrade_prompt)
{
    // Pre-release suffix on the base version is stripped before comparing.
    EXPECT_FALSE(UpdateChecker::isNewerVersion("2.0.0-rc1", "2.0.0"));  // same base -> no prompt
    // Garbage / empty tag must never be treated as newer.
    EXPECT_FALSE(UpdateChecker::isNewerVersion("garbage", "2.0.0"));
    EXPECT_FALSE(UpdateChecker::isNewerVersion("", "2.0.0"));
}
