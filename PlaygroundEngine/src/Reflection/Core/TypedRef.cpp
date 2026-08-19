module PlaygroundEngine.Reflection.Core;

import :TypedRef;
import :TypeInfo;

import std;

namespace PgE
{
	std::expected<TypedRef, DereferenceError> TypedRef::Dereference() const
	{
		if (Type == nullptr || Type->GetKind() != TypeKind::Pointer)
		{
			return std::unexpected(DereferenceError{DereferenceError::NotAPointer});
		}

		// Peel the pointee's cv nodes: a cv node has no structure of its own, so the borrow is only usable
		// once it names the unqualified type. Ex.: a const Foo* yields {.Type = Foo, .IsConst = true}, while
		// a Foo* const yields false, since that const is the pointer's own.
		const TypeInfo* pointeeType = &Type->GetInnerType();
		bool pointeeIsConst = false;
		while (pointeeType->GetTraits().IsConst || pointeeType->GetTraits().IsVolatile)
		{
			pointeeIsConst = pointeeIsConst || pointeeType->GetTraits().IsConst;
			pointeeType = &pointeeType->GetInnerType();
		}

		// What the pointer points AT is decided before its value is read, so a null function pointer answers
		// that it names no object rather than reporting itself as an expandable null.
		if (pointeeType->GetKind() == TypeKind::Function)
		{
			return std::unexpected(DereferenceError{DereferenceError::NotAnObjectPointer});
		}

		if (Data == nullptr)
		{
			return std::unexpected(DereferenceError{DereferenceError::NullObject});
		}

		// A pointer object holds an address, and reading it through void** would be an aliasing violation,
		// so the bytes are copied out rather than reinterpreted.
		void* pointee = nullptr;
		std::memcpy(&pointee, Data, sizeof(pointee));
		if (pointee == nullptr)
		{
			return std::unexpected(DereferenceError{DereferenceError::NullPointer});
		}

		// A pointee reached through someone else's pointer is a borrow, never an offer to move out of it.
		return TypedRef{.Type = pointeeType, .Data = pointee, .IsConst = pointeeIsConst, .Movable = false};
	}
}
