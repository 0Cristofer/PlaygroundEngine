#pragma once

// ImGui's context is opaque, so its module binding cannot export it, yet ImGui's hooks take one.
// This exists so the debug UI can name it: a global module fragment may hold only what an inclusion
// put there, and declaring it inside the module would make a different type.

struct ImGuiContext;
