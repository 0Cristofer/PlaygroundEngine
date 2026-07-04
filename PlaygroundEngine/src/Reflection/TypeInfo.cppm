export module PlaygroundEngine.Reflection:TypeInfo;

import PlaygroundEngine.Reflection.TypeInfoTraits;

import :FieldInfo;
import :FuncInfo;
import :TypedRef;
import :DeclarationInfo;

import std;

namespace PgE
{
	export template <typename T>
	constexpr const TypeInfo& TypeOf();

	export class TypeInfo : public DeclarationInfo
	{
	public:
		constexpr TypeInfo(const std::string_view identifier, const std::string_view displayName,
		                   const std::span<const FieldInfo> fields,
		                   const std::span<const FuncInfo> functions,
		                   std::string (*stringifyThunk)(const void*),
		                   const std::span<const AnnotationInfo> annotations) :
			DeclarationInfo(identifier, displayName, annotations),
			_fields(fields),
			_functions(functions),
			_stringifyThunk(stringifyThunk)
		{
		}

		std::string ObjectToString(const void* obj) const;
		std::string FunctionsToString() const;
		std::span<const FuncInfo> GetFunctions() const;
		std::vector<const FuncInfo*> FindFunctionsByIdentifier(std::string_view identifier) const;

		const FieldInfo* FindFieldByIdentifier(std::string_view identifier) const;

		std::expected<void, FieldError> GetFieldValue(const void* obj, std::string_view identifier, TypedRef out) const;
		std::expected<void, FieldError> SetFieldValue(void* obj, std::string_view identifier, TypedRef in) const;

		std::expected<TypedRef, FieldError> GetFieldRef(void* obj, std::string_view identifier) const;
		std::expected<TypedRef, FieldError> GetFieldRef(const void* obj, std::string_view identifier) const;

		template <typename T>
		std::expected<T, FieldError> GetFieldAs(const void* obj, const std::string_view identifier) const
		{
			const FieldInfo* field = FindFieldByIdentifier(identifier);
			if (!field)
				return std::unexpected(FieldError{FieldError::FieldNotFound});

			return field->GetAs<T>(obj);
		}

		template <typename T>
		std::expected<void, FieldError> SetFieldAs(void* obj, const std::string_view identifier, const T& value) const
		{
			const FieldInfo* field = FindFieldByIdentifier(identifier);
			if (!field)
				return std::unexpected(FieldError{FieldError::FieldNotFound});

			return field->SetAs<T>(obj, value);
		}

		template <typename T>
		std::expected<void, FieldError> MoveFieldAs(void* obj, const std::string_view identifier, T& value) const
		{
			const FieldInfo* field = FindFieldByIdentifier(identifier);
			if (!field)
				return std::unexpected(FieldError{FieldError::FieldNotFound});

			return field->MoveAs<T>(obj, value);
		}

		template <typename T>
		std::expected<std::reference_wrapper<T>, FieldError> GetFieldRefAs(void* obj, std::string_view identifier) const
		{
			const FieldInfo* field = FindFieldByIdentifier(identifier);
			if (!field)
				return std::unexpected(FieldError{FieldError::FieldNotFound});

			return field->GetRefAs<T>(obj);
		}

		template <typename T>
		std::expected<std::reference_wrapper<const T>, FieldError> GetFieldRefAs(const void* obj,
			const std::string_view identifier) const
		{
			const FieldInfo* field = FindFieldByIdentifier(identifier);
			if (!field)
				return std::unexpected(FieldError{FieldError::FieldNotFound});

			return field->GetRefAs<T>(obj);
		}

	private:
		std::span<const FieldInfo> _fields;
		std::span<const FuncInfo> _functions;
		std::string (*_stringifyThunk)(const void*) = nullptr;
	};
}
