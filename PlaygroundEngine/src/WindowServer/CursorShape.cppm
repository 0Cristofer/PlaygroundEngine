export module PlaygroundEngine.WindowServer:CursorShape;

import std;

namespace PgE
{
	/// What the pointer looks like over a region. Hidden belongs here rather than in a mode of its
	/// own because it is a thing a region asks for, unlike pointer lock, which is a mode the input
	/// layer enters and which changes what motion events mean.
	export enum class CursorShape : std::uint8_t
	{
		Arrow,
		TextInput,
		Hand,
		NotAllowed,
		ResizeHorizontal,
		ResizeVertical,
		ResizeTopLeftBottomRight,
		ResizeTopRightBottomLeft,
		ResizeAll,
		Hidden,
	};
}
