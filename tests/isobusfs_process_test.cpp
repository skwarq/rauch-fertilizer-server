#include "isobusfs_process.hpp"

#include <chrono>
#include <filesystem>
#include <gtest/gtest.h>
#include <string>
#include <utility>

TEST(IsobusFsProcess, StopTerminatesAndReapsChildProcess)
{
    const auto directory = std::filesystem::temp_directory_path()
                           / ("rauch-isobusfs-process-test-"
                              + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    rauch::IsobusFsConfig config;
    config.directory  = directory;
    config.executable = RAUCH_TEST_PROCESS_PATH;

    rauch::IsobusFsProcess server(std::move(config));
    server.start();
    ASSERT_TRUE(server.is_running());

    server.stop();

    EXPECT_FALSE(server.is_running());
    std::filesystem::remove_all(directory);
}

TEST(IsobusFsProcess, ReportsMissingExecutableBeforeCreatingOutputDirectory)
{
    const auto directory = std::filesystem::temp_directory_path()
                           / ("rauch-isobusfs-missing-test-"
                              + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    rauch::IsobusFsConfig config;
    config.directory  = directory;
    config.executable = directory / "does-not-exist";
    rauch::IsobusFsProcess server(std::move(config));

    try
    {
        server.start();
        FAIL() << "Expected a missing executable error";
    }
    catch (const std::runtime_error& error)
    {
        EXPECT_NE(std::string(error.what()).find("isobusfs-srv executable does not exist"), std::string::npos);
    }
    EXPECT_FALSE(std::filesystem::exists(directory));
}
