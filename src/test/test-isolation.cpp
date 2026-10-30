//////////////////////////////////////////////////////////////////////////////
//
// WordTsar - Wordstar clone for modern systems http://wordtsar.ca
// Copyright (C) 2026 Egbert H. Schroeer
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Affero General Public License as
// published by the Free Software Foundation, either version 3 of the
// License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Affero General Public License for more details.
//
// You should have received a copy of the GNU Affero General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//////////////////////////////////////////////////////////////////////////////

#include "doctest.h"
#include "src/core/utils/config.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>

#include <pwd.h>
#include <unistd.h>


namespace
{

bool StartsWith(const std::string& text, const std::string& prefix)
{
    return !prefix.empty() && text.compare(0, prefix.size(), prefix) == 0;
}

std::string PasswdHome()
{
    const passwd* pw = getpwuid(getuid());
    return (pw != nullptr && pw->pw_dir != nullptr) ? std::string(pw->pw_dir) : std::string();
}


/////////////////////////////////////////////////////////////////////////////
///
/// @brief Points HOME and XDG_CONFIG_HOME at a fresh temporary folder for
/// the whole test process and removes that folder when the process exits.
/// The HOME in effect at startup is kept in WORDTSAR_TEST_REAL_HOME.
///
/////////////////////////////////////////////////////////////////////////////
class TestHomeIsolation
{
public:
    TestHomeIsolation()
    {
        const char* home = std::getenv("HOME");
        setenv("WORDTSAR_TEST_REAL_HOME", home != nullptr ? home : PasswdHome().c_str(), 1);

        std::string pattern =
            (std::filesystem::temp_directory_path() / "wordtsar-test-home-XXXXXX").string();
        if (mkdtemp(pattern.data()) == nullptr)
        {
            std::perror("wordtsar test: mkdtemp failed");
            std::abort();
        }
        mDir = pattern;

        const std::filesystem::path xdg = mDir / ".config";
        std::filesystem::create_directories(xdg);

        setenv("HOME", mDir.c_str(), 1);
        setenv("XDG_CONFIG_HOME", xdg.c_str(), 1);
    }

    ~TestHomeIsolation()
    {
        std::error_code ec;
        std::filesystem::remove_all(mDir, ec);
    }

    TestHomeIsolation(const TestHomeIsolation&) = delete;
    TestHomeIsolation& operator=(const TestHomeIsolation&) = delete;

private:
    std::filesystem::path mDir;
};

const TestHomeIsolation gTestHomeIsolation;

} // namespace


TEST_CASE("Test process config directory is isolated from the real HOME")
{
    const char* realHomeEnv = std::getenv("WORDTSAR_TEST_REAL_HOME");
    CHECK(realHomeEnv != nullptr);

    const std::string realHome = realHomeEnv != nullptr ? std::string(realHomeEnv) : PasswdHome();
    const std::string tempRoot = std::filesystem::temp_directory_path().string();
    const std::string configDir = cConfig::GetConfigDir();
    const std::string configFile = cConfig::GetConfigFilePath();

    INFO("real HOME: " << realHome);
    INFO("temp root: " << tempRoot);
    INFO("config dir: " << configDir);
    INFO("config file: " << configFile);

    CHECK_FALSE(StartsWith(configDir, realHome));
    CHECK_FALSE(StartsWith(configFile, realHome));
    CHECK_FALSE(StartsWith(configFile, PasswdHome()));
    CHECK(StartsWith(configDir, tempRoot));
    CHECK(StartsWith(configFile, tempRoot));
}
