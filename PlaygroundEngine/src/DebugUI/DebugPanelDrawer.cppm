export module PlaygroundEngine.DebugUi:DebugPanelDrawer;

export import PlaygroundEngine.DebugUi.Annotations;

import PlaygroundEngine.Reflection;

import std;

namespace PgE
{
	/// Draws an object's annotated fields as ImGui rows, recursing through subobjects, pointers and
	/// containers. Rows only, so the caller owns the window. Subobjects are tree nodes visited only while
	/// open, so a reference cycle costs one level per click and needs no visited set.
	export class DebugPanelDrawer
	{
	public:
		template <typename T>
		requires(!std::same_as<std::remove_cvref_t<T>, TypedRef>)
		static void Draw(T& object)
		{
			DrawObject(TypedRefOf(object));
		}

		// The erased entry point, for a caller holding a borrow rather than a typed object: an entity's
		// components reach the panel this way, so it names no component type.
		static void Draw(const TypedRef& object)
		{
			DrawObject(object);
		}

	private:
		static void DrawObject(const TypedRef& object);
	};
}
