module PlaygroundEngine.Reflection;

import :FieldInfo;
import :FunctionInfo;
import :TypedRef;

import std;

namespace PgE
{
	namespace
	{
		std::string FieldToString(const FieldInfo& field, const void* obj)
		{
			if (const auto ref = field.GetRef(obj))
				return field.GetTypeInfo().ObjectToString(ref->Data);

			alignas(std::uintmax_t) std::byte slot[sizeof(std::uintmax_t)];
			if (field.GetValue(obj, TypedRef{.Type = &field.GetTypeInfo(), .Data = slot, .IsConst = false}))
				return field.GetTypeInfo().ObjectToString(slot);

			return "<unreadable>";
		}
	}

	std::string TypeInfo::ObjectToString(const void* obj) const
	{
		if (_stringifyThunk)
			return _stringifyThunk(obj);

		std::string out = "{";
		bool firstField = true;
		for (const FieldInfo& field : _fields)
		{
			if (!firstField)
				out += ", ";
			firstField = false;
			out += field.GetIdentifier();
			out += ": ";
			out += FieldToString(field, obj);
		}
		return out + "}";
	}

	std::string TypeInfo::FunctionsToString() const
	{
		std::string out;
		bool firstFunc = true;
		for (const FunctionInfo& function : _functions)
		{
			if (!firstFunc)
				out += '\n';
			firstFunc = false;
			out += function.GetReturnType().GetDisplayName();
			out += " ";
			out += function.GetIdentifier();
			out += "(";

			bool firstParam = true;
			for (const ParameterInfo& param : function.GetParams())
			{
				if (!firstParam)
					out += ", ";
				firstParam = false;

				out += param.GetTypeInfo().GetDisplayName();
				if (!param.GetIdentifier().empty())
				{
					out += " ";
					out += param.GetIdentifier();
				}
			}

			out += ")";
		}
		return out;
	}

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

	// ReSharper disable CppPassValueParameterByConstReference

	std::expected<void, FieldError> TypeInfo::GetFieldValue(const void* obj, const std::string_view identifier,
	                                                        const TypedRef out) const
	{
		const FieldInfo* field = FindFieldByIdentifier(identifier);
		if (!field)
			return std::unexpected(FieldError{FieldError::FieldNotFound});

		return field->GetValue(obj, out);
	}

	std::expected<void, FieldError> TypeInfo::SetFieldValue(void* obj, const std::string_view identifier,
	                                                        const TypedRef in) const
	{
		const FieldInfo* field = FindFieldByIdentifier(identifier);
		if (!field)
			return std::unexpected(FieldError{FieldError::FieldNotFound});

		return field->SetValue(obj, in);
	}

	// ReSharper restore CppPassValueParameterByConstReference

	std::expected<TypedRef, FieldError> TypeInfo::GetFieldRef(void* obj, const std::string_view identifier) const
	{
		const FieldInfo* field = FindFieldByIdentifier(identifier);
		if (!field)
			return std::unexpected(FieldError{FieldError::FieldNotFound});

		return field->GetRef(obj);
	}

	std::expected<TypedRef, FieldError> TypeInfo::GetFieldRef(const void* obj, const std::string_view identifier) const
	{
		const FieldInfo* field = FindFieldByIdentifier(identifier);
		if (!field)
			return std::unexpected(FieldError{FieldError::FieldNotFound});

		return field->GetRef(obj);
	}
}
