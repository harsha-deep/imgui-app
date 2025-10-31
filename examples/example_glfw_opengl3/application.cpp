#include "imgui.h"
#include "application.h"

namespace myApp
{
    void renderUI()
    {
        // Example UI code
        ImGui::Begin("Hello, world!");            // Create a window called "Hello, world!" and append into it.
        ImGui::Text("This is some useful text."); // Display some text (you can use a format strings too)
        ImGui::Button("Hello.");                  // Display some text (you can use a format strings too)
        static float f = 0.0f;
        ImGui::DragFloat("float", &f);
        ImGui::End();
    }
}