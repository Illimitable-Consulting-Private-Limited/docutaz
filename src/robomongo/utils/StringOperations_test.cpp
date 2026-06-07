#include "gtest/gtest.h"
#include "robomongo/utils/StringOperations.h"

#include <string>

/* Example Test:
 *
 * TEST( [Test_Case_Name], [Test_Name] )
 * TEST( [Test_Case_Name], [UnitOfWorkName_ScenarioUnderTest_ExpectedBehavior] )
 * TEST( StringParserTests, NumberLeftOf_StringWithoutNumber_ReturnsFalse) {
    // ...
   }
*/

TEST(StringOperationsTests, captilizeFirstChar)
{
    // EXPECT_EQ("Abcc", Docutaz::captilizeFirstChar("abc")); // Simulating failing test
    EXPECT_EQ("Abc", Docutaz::captilizeFirstChar("abc")); // Simulating passing test
}