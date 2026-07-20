export module PlaygroundEngine.Reflection.Core:DestructorInfo;

import :DeclarationInfo;

import std;

namespace PgE
{
	export struct DestructorTraits
	{
		// The language facts of a type's one destructor. IsTrivial is the whole-type is_trivially_destructible,
		// surfaced here so the lifetime-end story is answered from the destructor alone; IsDefaulted separates a
		// compiler-generated destructor from a user-written one.
		AccessKind Access = AccessKind::Public;

		bool IsVirtual = false;
		bool IsPureVirtual = false;
		bool IsDeleted = false;
		bool IsDefaulted = false;
		bool IsTrivial = false;
	};

	// A type's single destructor: its lifetime-end metadata plus the erased destroy thunk. A type with no
	// reachable destructor (a reference, an incomplete type, an array) reflects one with a null thunk, the way
	// a constructor reflects with no thunk. See docs/ReflectionInternals.md (lifetime ops).
	export class DestructorInfo : public DeclarationInfo
	{
	public:
		using Destroyer = void (*)(void*);

		constexpr DestructorInfo(const std::string_view displayName,
								 const std::span<const std::string_view> scopePath,
								 const DestructorTraits& traits,
								 const Destroyer destroy,
								 const std::span<const AnnotationInfo> annotations)
			: DeclarationInfo({}, displayName, scopePath, annotations), _traits(traits), _destroy(destroy)
		{}

		const DestructorTraits& GetTraits() const
		{
			return _traits;
		}
		AccessKind GetAccess() const
		{
			return _traits.Access;
		}
		bool IsVirtual() const
		{
			return _traits.IsVirtual;
		}
		bool IsPureVirtual() const
		{
			return _traits.IsPureVirtual;
		}
		bool IsDeleted() const
		{
			return _traits.IsDeleted;
		}
		bool IsDefaulted() const
		{
			return _traits.IsDefaulted;
		}
		bool IsTrivial() const
		{
			return _traits.IsTrivial;
		}

		// A destructor whose erased path is unusable (deleted, inaccessible, or a type with no destructor)
		// reflects as metadata with no thunk; the traits name which, the way ConstructorInfo does.
		bool CanDestroy() const
		{
			return _destroy != nullptr;
		}
		void Destroy(void* obj) const pre(_destroy != nullptr) pre(obj != nullptr)
		{
			_destroy(obj);
		}

	private:
		DestructorTraits _traits;

		// A trivial destructor is set at the metadata build (its ~T has no body to splice); a non-trivial one is
		// set on demand, when the type is named through TypeOf<T>. Null for a type with no reachable destructor.
		Destroyer _destroy = nullptr;

		// Sets the thunk in place: a trivial one at the metadata build, a non-trivial one on demand. A hidden
		// friend rather than a public setter, so the thunk stays builder-written (reached only through ADL).
		friend void SetDestroyer(DestructorInfo& destructor, const Destroyer destroy)
		{
			destructor._destroy = destroy;
		}
	};
}
