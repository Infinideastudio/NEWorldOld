#pragma once

struct GLFWwindow;

void splashScreen();

void createWindow();

void ToggleFullScreen();

void setupScreen();

void loadTextures();

void WindowSizeFunc(GLFWwindow* win, int width, int height);

void MouseButtonFunc(GLFWwindow*, int button, int action, int);

void CharInputFunc(GLFWwindow*, unsigned int c);

void MouseScrollFunc(GLFWwindow*, double, double yoffset);
