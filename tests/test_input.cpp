#include <gtest/gtest.h>
#include "Input.h"

TEST(InputTest, WasPressedOnlyInTheFirstFrame) {
    Input input;
    input.SetKey(Key::Space, true);
    EXPECT_TRUE(input.IsDown(Key::Space));
    EXPECT_TRUE(input.WasPressed(Key::Space));

    input.EndFrame();
    EXPECT_TRUE(input.IsDown(Key::Space));
    EXPECT_FALSE(input.WasPressed(Key::Space));

    input.SetKey(Key::Space, false);
    input.EndFrame();
    input.SetKey(Key::Space, true);
    EXPECT_TRUE(input.WasPressed(Key::Space));
}

TEST(InputTest, MouseDeltasResetEveryFrame) {
    Input input;
    input.SetMousePosition(10.0f, 10.0f);
    input.MoveMouse(15.0f, 8.0f);
    input.Scroll(1.0f);
    EXPECT_FLOAT_EQ(input.MouseDeltaX(), 5.0f);
    EXPECT_FLOAT_EQ(input.MouseDeltaY(), -2.0f);
    EXPECT_FLOAT_EQ(input.ScrollDelta(), 1.0f);

    input.EndFrame();
    EXPECT_FLOAT_EQ(input.MouseDeltaX(), 0.0f);
    EXPECT_FLOAT_EQ(input.ScrollDelta(), 0.0f);
    EXPECT_FLOAT_EQ(input.MouseX(), 15.0f);
}

TEST(InputTest, ReleaseAll) {
    Input input;
    input.SetKey(Key::W, true);
    input.SetMouseButton(MouseButton::Right, true);
    input.ReleaseAll();
    EXPECT_FALSE(input.IsDown(Key::W));
    EXPECT_FALSE(input.IsDown(MouseButton::Right));
}

TEST(InputTest, MouseWasPressedOnlyInTheFirstFrame) {
    Input input;
    input.SetMouseButton(MouseButton::Left, true);
    EXPECT_TRUE(input.WasPressed(MouseButton::Left));
    input.EndFrame();
    EXPECT_TRUE(input.IsDown(MouseButton::Left));
    EXPECT_FALSE(input.WasPressed(MouseButton::Left));
}

TEST(InputTest, WindowSize) {
    Input input;
    input.SetWindowSize(1280.0f, 720.0f);
    EXPECT_FLOAT_EQ(input.WindowWidth(), 1280.0f);
    EXPECT_FLOAT_EQ(input.WindowHeight(), 720.0f);
}
