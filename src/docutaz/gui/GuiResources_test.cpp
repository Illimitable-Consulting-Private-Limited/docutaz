#include "gtest/gtest.h"

#include <QFile>

// Regression test for the loading-animation resource paths.
//
// After the project was renamed, the .qrc prefix and every QIcon path moved to
// ":/docutaz/...", but three QMovie animations were left pointing at the old
// ":robomongo/icons/..." prefix, so they silently resolved to nothing. These
// assert the gifs are registered at the path the code now uses. (Qt resources
// are available via QFile without a QApplication; the OBJECT-library build
// links docutaz_core's AUTORCC initializers into the test binary.)

TEST(gui_resources, loading_animations_resolve_under_docutaz_prefix)
{
    EXPECT_TRUE(QFile(":/docutaz/icons/progress_bar.gif").exists());
    EXPECT_TRUE(QFile(":/docutaz/icons/loading.gif").exists());
    EXPECT_TRUE(QFile(":/docutaz/icons/loading_ticks_40x40.gif").exists());
}
