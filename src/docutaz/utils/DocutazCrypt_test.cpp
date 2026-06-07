#include "gtest/gtest.h"
#include "docutaz/utils/DocutazCrypt.h"

#include <string>

/* Example Test:
 *
 * TEST( [Test_Case_Name], [Test_Name] )
 * TEST( [Test_Case_Name], [UnitOfWorkName_ScenarioUnderTest_ExpectedBehavior] )
 * TEST( StringParserTests, NumberLeftOf_StringWithoutNumber_ReturnsFalse) {
    // ...
   }
*/

TEST(DocutazCrypt_CoreTests, encrypt_decrypt)
{  
    auto const pwds = {
        "Tyu_aBq",
        "_?asdfghjkl;'piop[.,/",
        ".?/`_@~!#$%^^&&*)_)_+=-",        
        "<>?/.,;':][p{}|\""
    };
    for (auto const& pwd : pwds) {
        const std::string encryptedPwd = Docutaz::DocutazCrypt::encrypt(pwd);
        const std::string decryptedPwd = Docutaz::DocutazCrypt::decrypt(encryptedPwd);
        EXPECT_EQ(pwd, decryptedPwd);
    }
}