module PlaygroundEngine.WindowServer:GlfwInputTranslation;

import PlaygroundEngine.PlatformEvents;

namespace PgE
{
	// GLFW key tokens are already physical positions, so this is a straight relabelling rather than
	// a layout translation. The tables themselves are in the implementation unit, which is also what
	// keeps GLFW's header out of this partition entirely.

	InputCode TranslateGlfwKey(int key);
	InputCode TranslateGlfwPointerButton(int button);
	InputModifiers TranslateGlfwModifiers(int modifiers);
	InputLockState TranslateGlfwLockState(int modifiers);
}
