export module PlaygroundEngine.DebugUi:DebugPanelDrawer;

import PlaygroundEngine.Reflection;

import std;

namespace PgE
{
	/// Marks a field as visible to DebugPanelDrawer. Required at every level: a subobject draws only the
	/// fields carrying it, so a panel never widens because a type grew a member somewhere below.
	export struct DrawDebug
	{};

	/// Draws an object's annotated fields as ImGui rows, recursing through subobjects, pointers and
	/// containers. Rows only, so the caller owns the window. Subobjects are tree nodes visited only while
	/// open, so a reference cycle costs one level per click and needs no visited set.
	export class DebugPanelDrawer
	{
	public:
		template <typename T>
		static void Draw(T& object)
		{
			// Sound against a read-only walk: IsConst reaches every row below and no writer runs behind a
			// disabled widget. The same bargain FieldInfo::GetRef(const void*) makes for its erased borrow.
			DrawObject(TypedRefOf(object));
		}

	private:
		static void DrawObject(TypedRef object);
	};
}
