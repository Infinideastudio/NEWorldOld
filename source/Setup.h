#pragma once

struct GLFWwindow;

void createWindow();

void toggleFullScreen();

void setupScreen();

void loadTextures();

void WindowSizeFunc(GLFWwindow* win, int width, int height);

void MouseButtonFunc(GLFWwindow*, int button, int action, int);

void CharInputFunc(GLFWwindow*, unsigned int c);

void MouseScrollFunc(GLFWwindow*, double, double yoffset);

void splashScreen();
