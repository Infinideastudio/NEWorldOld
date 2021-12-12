#pragma once

#include "Definitions.h"
#include "Globalization.h"
#include <NsCore/Ptr.h>
#include <NsGui/IView.h>

namespace GUI {
    class Scene {
    public:
        Scene(const char* xaml, bool hasCursor = true) : mXaml(xaml), mHasCursor(hasCursor) {}

        virtual ~Scene();

        void load();
        void singleLoop();

    protected:
        virtual void onRender() {}
        virtual void onUpdate() {}
        virtual void onLoad() {}

        Noesis::Ptr<Noesis::IView> _view;

    private:
        void render();
        void update();

        const char* mXaml;
        bool mHasCursor;
    };

    void pushScene(std::unique_ptr<Scene> scene);

    void popScene();

    void clearScenes();

    void appStart();
}