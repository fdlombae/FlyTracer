#include <gtest/gtest.h>
#include "Config.h"
#include <fstream>
#include <cstdio>

using namespace FlyTracer;

class ConfigTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create a test config file
        std::ofstream file(testConfigPath);
        file << "# Test config\n";
        file << "window_width=800\n";
        file << "window_height=600\n";
        file << "window_title=TestApp\n";
        file << "fullscreen=true\n";
        file << "vsync=false\n";
        file << "max_ray_bounces=8\n";
        file << "resource_directory=test_resources\n";
        file << "camera_fov=90.0\n";
        file.close();
    }

    void TearDown() override {
        std::remove(testConfigPath.c_str());
        std::remove(testOutputPath.c_str());
    }

    std::string testConfigPath = "/tmp/test_config.cfg";
    std::string testOutputPath = "/tmp/test_output.cfg";
};

// Test default values
TEST_F(ConfigTest, DefaultValues) {
    AppConfig config;

    EXPECT_EQ(config.windowWidth, 1280);
    EXPECT_EQ(config.windowHeight, 720);
    EXPECT_EQ(config.windowTitle, "FlyTracer");
    EXPECT_FALSE(config.fullscreen);
    EXPECT_TRUE(config.vsync);
    EXPECT_EQ(config.maxRayBounces, 3);
    EXPECT_EQ(config.resourceDirectory, "resources");
    EXPECT_FLOAT_EQ(config.cameraFov, 60.0f);
}

// Test loading from file
TEST_F(ConfigTest, LoadFromFile) {
    AppConfig config;
    bool result = config.loadFromFile(testConfigPath);

    EXPECT_TRUE(result);
    EXPECT_EQ(config.windowWidth, 800);
    EXPECT_EQ(config.windowHeight, 600);
    EXPECT_EQ(config.windowTitle, "TestApp");
    EXPECT_TRUE(config.fullscreen);
    EXPECT_FALSE(config.vsync);
    EXPECT_EQ(config.maxRayBounces, 8);
    EXPECT_EQ(config.resourceDirectory, "test_resources");
    EXPECT_FLOAT_EQ(config.cameraFov, 90.0f);
}

// Test loading from non-existent file returns false
TEST_F(ConfigTest, LoadFromNonExistentFile) {
    AppConfig config;
    bool result = config.loadFromFile("/tmp/nonexistent_config.cfg");

    EXPECT_FALSE(result);
    // Should keep default values
    EXPECT_EQ(config.windowWidth, 1280);
    EXPECT_EQ(config.windowHeight, 720);
}

// Test saving to file
TEST_F(ConfigTest, SaveToFile) {
    AppConfig config;
    config.windowWidth = 1920;
    config.windowHeight = 1080;
    config.windowTitle = "SavedConfig";
    config.maxRayBounces = 6;

    bool saveResult = config.saveToFile(testOutputPath);
    EXPECT_TRUE(saveResult);

    // Load it back and verify
    AppConfig loadedConfig;
    bool loadResult = loadedConfig.loadFromFile(testOutputPath);
    EXPECT_TRUE(loadResult);
    EXPECT_EQ(loadedConfig.windowWidth, 1920);
    EXPECT_EQ(loadedConfig.windowHeight, 1080);
    EXPECT_EQ(loadedConfig.windowTitle, "SavedConfig");
    EXPECT_EQ(loadedConfig.maxRayBounces, 6);
}

// Test parsing with whitespace
TEST_F(ConfigTest, ParseWithWhitespace) {
    std::ofstream file("/tmp/whitespace_config.cfg");
    file << "  window_width  =  640  \n";
    file << "\twindow_height\t=\t480\t\n";
    file.close();

    AppConfig config;
    (void)config.loadFromFile("/tmp/whitespace_config.cfg");

    EXPECT_EQ(config.windowWidth, 640);
    EXPECT_EQ(config.windowHeight, 480);

    std::remove("/tmp/whitespace_config.cfg");
}

// Test comments are ignored
TEST_F(ConfigTest, CommentsIgnored) {
    std::ofstream file("/tmp/comments_config.cfg");
    file << "# This is a comment\n";
    file << "; This is also a comment\n";
    file << "window_width=500\n";
    file << "# Another comment\n";
    file.close();

    AppConfig config;
    (void)config.loadFromFile("/tmp/comments_config.cfg");

    EXPECT_EQ(config.windowWidth, 500);

    std::remove("/tmp/comments_config.cfg");
}

// Test boolean parsing
TEST_F(ConfigTest, BooleanParsing) {
    std::ofstream file("/tmp/bool_config.cfg");
    file << "fullscreen=1\n";
    file << "vsync=false\n";
    file.close();

    AppConfig config;
    (void)config.loadFromFile("/tmp/bool_config.cfg");

    EXPECT_TRUE(config.fullscreen);
    EXPECT_FALSE(config.vsync);

    std::remove("/tmp/bool_config.cfg");
}

// Test invalid numeric values don't crash (use defaults)
TEST_F(ConfigTest, InvalidNumericValues) {
    std::ofstream file("/tmp/invalid_config.cfg");
    file << "window_width=not_a_number\n";
    file << "window_height=600\n";
    file << "camera_fov=also_invalid\n";
    file.close();

    AppConfig config;
    bool result = config.loadFromFile("/tmp/invalid_config.cfg");

    // Should still return true (file loaded, just some values invalid)
    EXPECT_TRUE(result);
    // Invalid value should keep default
    EXPECT_EQ(config.windowWidth, 1280);  // Default
    // Valid value should be parsed
    EXPECT_EQ(config.windowHeight, 600);
    // Invalid float should keep default
    EXPECT_FLOAT_EQ(config.cameraFov, 60.0f);  // Default

    std::remove("/tmp/invalid_config.cfg");
}

// Test out of range numeric values
TEST_F(ConfigTest, OutOfRangeValues) {
    std::ofstream file("/tmp/range_config.cfg");
    // Value larger than int max
    file << "window_width=99999999999999999999\n";
    file << "window_height=600\n";
    file.close();

    AppConfig config;
    bool result = config.loadFromFile("/tmp/range_config.cfg");

    EXPECT_TRUE(result);
    // Out of range should keep default
    EXPECT_EQ(config.windowWidth, 1280);  // Default
    // Valid value should be parsed
    EXPECT_EQ(config.windowHeight, 600);

    std::remove("/tmp/range_config.cfg");
}

// Test lines without equals sign are skipped
TEST_F(ConfigTest, MalformedLinesSkipped) {
    std::ofstream file("/tmp/malformed_config.cfg");
    file << "this line has no equals\n";
    file << "window_width=800\n";
    file << "another bad line\n";
    file.close();

    AppConfig config;
    bool result = config.loadFromFile("/tmp/malformed_config.cfg");

    EXPECT_TRUE(result);
    EXPECT_EQ(config.windowWidth, 800);

    std::remove("/tmp/malformed_config.cfg");
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
