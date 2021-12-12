#include <deque>
#include <climits>
#include "GUI.h"
#include "TextRenderer.h"
#include "Frustum.h"
#include "AudioSystem.h"
#include <NsRender/GLFactory.h>
#include <NsApp/ThemeProviders.h>
#include <NoesisPCH.h>

namespace GUI {

    void Scene::update() {
        // TODO: change to use glfw callback + message bus?
        if (_view) {
            double xpos, ypos;
            glfwGetCursorPos(MainWindow, &xpos, &ypos);
            _view->MouseMove(xpos, ypos);
            _view->SetSize(windowwidth, windowheight);
            int state = glfwGetMouseButton(MainWindow, GLFW_MOUSE_BUTTON_LEFT);
            if (state == GLFW_PRESS) {
                _view->MouseButtonDown(xpos, ypos, Noesis::MouseButton_Left);
            }
            else {
                _view->MouseButtonUp(xpos, ypos, Noesis::MouseButton_Left);
            }
        }

        onUpdate();
    }

    void Scene::render() {

        // Update view (layout, animations, ...)
        _view->Update(1 / 30.0f);

        // Offscreen rendering phase populates textures needed by the on-screen rendering
        _view->GetRenderer()->UpdateRenderTree();
        _view->GetRenderer()->RenderOffscreen();

        // If you are going to render here with your own engine you need to restore the GPU state
        // because noesis changes it. In this case only framebuffer and viewport need to be restored
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, windowwidth, windowheight);

        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClearStencil(0);
        glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

        onRender();

        // Rendering is done in the active framebuffer
        _view->GetRenderer()->Render();
    }

    Scene::~Scene() {

    }

    void Scene::singleLoop() {
        update();
        render();
        glfwSwapBuffers(MainWindow);
        glfwPollEvents();
    }

    void Scene::load() {
        glfwSetInputMode(MainWindow, GLFW_CURSOR, mHasCursor ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_HIDDEN);
        if (mXaml) {
            _view = Noesis::GUI::CreateView(Noesis::GUI::LoadXaml<Noesis::Grid>("MainMenu.xaml"));
            _view->SetFlags(Noesis::RenderFlags_PPAA | Noesis::RenderFlags_LCD);
            _view->GetRenderer()->Init(NoesisApp::GLFactory::CreateDevice(false));
        }
        onLoad();
    }

    std::deque<std::unique_ptr<Scene>> scenes;
    
    void pushScene(std::unique_ptr<Scene> scene) {
        scene->load();
        scenes.emplace_back(std::move(scene));
    }

    void popScene() {
        scenes.pop_back();
    }

    void clearScenes() {
        while (!scenes.empty()) popScene();
    }

    void appStart() {
        glfwSetInputMode(MainWindow, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        glClearColor(0.0, 0.0, 0.0, 1.0);
        glDisable(GL_CULL_FACE);
        TextRenderer::setFontColor(1.0, 1.0, 1.0, 1.0);

        while (!scenes.empty()) {
            (*scenes.begin())->singleLoop();
            if (glfwWindowShouldClose(MainWindow)) {
                clearScenes();
            }
        }
        AppCleanUp();
    }
}