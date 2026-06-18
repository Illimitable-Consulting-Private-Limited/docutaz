// GUI test harness entry point.
//
// GUI tests drive real Docutaz widgets (dialogs, the editor) with Qt Test
// helpers (QTest input simulation, QSignalSpy) and assert with Google Test.
// They need a QApplication, created here before any widget, on the offscreen
// platform so the suite runs headless in CI.
#include <QApplication>

#include "gtest/gtest.h"

int main(int argc, char **argv)
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
