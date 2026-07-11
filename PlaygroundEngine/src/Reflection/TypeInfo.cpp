module PlaygroundEngine.Reflection;

import :FieldInfo;
import :FunctionInfo;
import :TypedRef;
import :Facets;

import std;

namespace PgE
{
	std::span<const FunctionInfo> TypeInfo::GetFunctions() const
	{
		return _functions;
	}

	std::vector<const FunctionInfo*> TypeInfo::FindFunctionsByIdentifier(const std::string_view identifier) const
	{
		// Linear scan: function counts per type are small and lookups happen at boundaries, not the
		// frame loop. Acceleration, if ever needed, belongs at the registry keyed by stable id.
		std::vector<const FunctionInfo*> matches;
		for (const FunctionInfo& function : _functions)
		{
			if (function.GetIdentifier() == identifier)
				matches.push_back(&function);
		}

		return matches;
	}

	const FieldInfo* TypeInfo::FindFieldByIdentifier(const std::string_view identifier) const
	{
		// Fields are unique by identifier, so the first match is the only one. Linear scan, same rationale
		// as FindFunctionsByIdentifier.
		for (const FieldInfo& field : _fields)
		{
			if (field.GetIdentifier() == identifier)
				return &field;
		}

		return nullptr;
	}


	std::expected<void, FieldError> TypeInfo::GetFieldValue(const void* obj, const std::string_view identifier, const TypedRef& out) const
	{
		const FieldInfo* field = FindFieldByIdentifier(identifier);
		if (!field)
		{
			return std::unexpected(FieldError{FieldError::FieldNotFound});
		}

		return field->GetValue(obj, out);
	}

	std::expected<void, FieldError> TypeInfo::SetFieldValue(void* obj, const std::string_view identifier, const TypedRef& in) const
	{
		const FieldInfo* field = FindFieldByIdentifier(identifier);
		if (!field)
		{
			return std::unexpected(FieldError{FieldError::FieldNotFound});
		}

		return field->SetValue(obj, in);
	}

	std::expected<TypedRef, FieldError> TypeInfo::GetFieldRef(void* obj, const std::string_view identifier) const
	{
		const FieldInfo* field = FindFieldByIdentifier(identifier);
		if (!field)
		{
			return std::unexpected(FieldError{FieldError::FieldNotFound});
		}

		return field->GetRef(obj);
	}

	std::expected<TypedRef, FieldError> TypeInfo::GetFieldRef(const void* obj, const std::string_view identifier) const
	{
		const FieldInfo* field = FindFieldByIdentifier(identifier);
		if (!field)
		{
			return std::unexpected(FieldError{FieldError::FieldNotFound});
		}

		return field->GetRef(obj);
	}
}
