#include <Geode/Geode.hpp>

#include "render/renderer.hpp"

using namespace geode::prelude;

#include <Geode/modify/CCEGLView.hpp>

struct SLCCEGLView : Modify<SLCCEGLView, CCEGLView> {
    void setFrameSize(float width, float height) {
        if (Renderer::get()->handleFrameSizeChange(width, height)) {
            return;
        }

        CCEGLView::setFrameSize(width, height);
    }

    void onGLFWframebuffersize(GLFWwindow* window, int width, int height) {
        if (Renderer::get()->handleFramebufferSizeChange(width, height)) {
            return;
        }

        CCEGLView::onGLFWframebuffersize(window, width, height);
    }

    void onGLFWWindowSizeFunCallback(GLFWwindow* window, int width,
                                     int height) {
        if (Renderer::get()->handleWindowSizeChange()) {
            return;
        }

        CCEGLView::onGLFWWindowSizeFunCallback(window, width, height);
    }
};
