#include <gtest/gtest.h>
#include "Config.h"
#include <filesystem>
#include <fstream>
#include <string>

using namespace FlyTracer;

class ConfigTest : public ::testing::Test {
protected:
    void TearDown() override {
        std::filesystem::remove(m_path);
    }

    // Writes the text to a config file and loads it
    AppConfig Load(const std::string& text) {
        std::ofstream(m_path) << text;
        AppConfig config;
        EXPECT_TRUE(config.loadFromFile(m_path.string()));
        return config;
    }

    const std::filesystem::path m_path = std::filesystem::temp_directory_path() / "flytracer_test_config.cfg";
};

TEST_F(ConfigTest, DefaultValues) {
    const AppConfig config;
    EXPECT_EQ(config.windowWidth, 1280);
    EXPECT_EQ(config.windowHeight, 720);
    EXPECT_EQ(config.windowTitle, "FlyTracer");
    EXPECT_FALSE(config.fullscreen);
    EXPECT_TRUE(config.vsync);
    EXPECT_EQ(config.maxRayBounces, 3);
    EXPECT_EQ(config.resourceDirectory, "resources");
    EXPECT_EQ(config.shaderDirectory, "shaders");
}

TEST_F(ConfigTest, LoadFromFile) {
    const AppConfig config = Load("# Test config\n"
                                  "window_width=800\n"
                                  "window_height=600\n"
                                  "window_title=TestApp\n"
                                  "fullscreen=true\n"
                                  "vsync=false\n"
                                  "max_ray_bounces=8\n"
                                  "resource_directory=test_resources\n"
                                  "unknown_key=ignored\n");
    EXPECT_EQ(config.windowWidth, 800);
    EXPECT_EQ(config.windowHeight, 600);
    EXPECT_EQ(config.windowTitle, "TestApp");
    EXPECT_TRUE(config.fullscreen);
    EXPECT_FALSE(config.vsync);
    EXPECT_EQ(config.maxRayBounces, 8);
    EXPECT_EQ(config.resourceDirectory, "test_resources");
}

// A missing file keeps the defaults
TEST_F(ConfigTest, LoadFromNonExistentFile) {
    AppConfig config;
    EXPECT_FALSE(config.loadFromFile((std::filesystem::temp_directory_path() / "flytracer_missing.cfg").string()));
    EXPECT_EQ(config.windowWidth, 1280);
    EXPECT_EQ(config.windowHeight, 720);
}

TEST_F(ConfigTest, SaveToFile) {
    AppConfig config;
    config.windowWidth = 1920;
    config.windowHeight = 1080;
    config.windowTitle = "SavedConfig";
    config.maxRayBounces = 6;
    ASSERT_TRUE(config.saveToFile(m_path.string()));

    AppConfig loaded;
    ASSERT_TRUE(loaded.loadFromFile(m_path.string()));
    EXPECT_EQ(loaded.windowWidth, 1920);
    EXPECT_EQ(loaded.windowHeight, 1080);
    EXPECT_EQ(loaded.windowTitle, "SavedConfig");
    EXPECT_EQ(loaded.maxRayBounces, 6);
}

TEST_F(ConfigTest, ParseWithWhitespace) {
    const AppConfig config = Load("  window_width  =  640  \n\twindow_height\t=\t480\t\n");
    EXPECT_EQ(config.windowWidth, 640);
    EXPECT_EQ(config.windowHeight, 480);
}

TEST_F(ConfigTest, CommentsIgnored) {
    const AppConfig config = Load("# A comment\n; Also a comment\nwindow_width=500\n# window_height=100\n");
    EXPECT_EQ(config.windowWidth, 500);
    EXPECT_EQ(config.windowHeight, 720);
}

TEST_F(ConfigTest, BooleanParsing) {
    const AppConfig config = Load("fullscreen=1\nvsync=false\n");
    EXPECT_TRUE(config.fullscreen);
    EXPECT_FALSE(config.vsync);
}

// Invalid and out of range numbers keep the defaults; the other lines are still read
TEST_F(ConfigTest, InvalidNumericValues) {
    const AppConfig config = Load("window_width=not_a_number\nwindow_height=600\nmax_ray_bounces=99999999999999999999\n");
    EXPECT_EQ(config.windowWidth, 1280);
    EXPECT_EQ(config.windowHeight, 600);
    EXPECT_EQ(config.maxRayBounces, 3);
}

TEST_F(ConfigTest, MalformedLinesSkipped) {
    const AppConfig config = Load("this line has no equals\nwindow_width=800\nanother bad line\n");
    EXPECT_EQ(config.windowWidth, 800);
}

// The whole string must be a number, as for the command line arguments
TEST(ParseNumberTest, WholeStringOnly) {
    int value = 7;
    EXPECT_TRUE(parseNumber("42", value));
    EXPECT_EQ(value, 42);
    EXPECT_FALSE(parseNumber("12abc", value));
    EXPECT_FALSE(parseNumber("", value));
    EXPECT_EQ(value, 42);
}
