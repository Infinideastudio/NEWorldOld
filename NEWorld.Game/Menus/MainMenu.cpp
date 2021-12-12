#include "Menus.h"
#include "TextRenderer.h"
#include "GUI.h"
#include "AudioSystem.h"
#include <NoesisPCH.h>
#include <NsRender/GLFactory.h>
#include <NsApp/ThemeProviders.h>

namespace Menus {
    class GameMenu : public GUI::Form {
    private:
        Noesis::Ptr<Noesis::IView> _view;

        void onUpdate() override
        {
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

        void onLoad() override {
            glfwSetInputMode(MainWindow, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

            _view = Noesis::GUI::CreateView(Noesis::GUI::LoadXaml<Noesis::Grid>("MainMenu.xaml"));
            _view->SetFlags(Noesis::RenderFlags_PPAA | Noesis::RenderFlags_LCD);
            _view->GetRenderer()->Init(NoesisApp::GLFactory::CreateDevice(false));
        }

        void onRender() override {
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

            // Rendering is done in the active framebuffer
            _view->GetRenderer()->Render();
        }
    };

}

GUI::Form *GUI::GetMain() {
    return new Menus::GameMenu;
}