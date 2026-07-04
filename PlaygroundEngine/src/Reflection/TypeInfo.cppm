export module PlaygroundEngine.Reflection:TypeInfo;

import PlaygroundEngine.Reflection.TypeInfoTraits;

import :FieldInfo;
import :FuncInfo;
import :TypedRef;
import :Annotation;

import std;

namespace PgE
{
	export template <typename T>
	constexpr const TypeInfo& TypeOf();

	export class TypeInfo : public Annotated
	{
	public:
		constexpr TypeInfo(const std::string_view name, const std::span<const FieldInfo> fields,
		                   const std::span<const FuncInfo> functions,
		                   std::string (*stringifyThunk)(const void*),
		                   const std::span<const AnnotationInfo> annotations) : Annotated(annotations),
			_displayName(name),
			_fields(fields),
			_functions(functions),
			_stringifyThunk(stringifyThunk)
		{
		}

		std::string_view GetDisplayName() const
		{
			return _displayName;
		}

		std::string ObjectToString(const void* obj) const;
		std::string FunctionsToString() const;
		std::span<const FuncInfo> GetFunctions() const;
		std::vector<const FuncInfo*> FindFunctionsByName(std::string_view name) const;

		const FieldInfo* FindFieldByName(std::string_view name) const;

		std::expected<void, FieldError> GetFieldValue(const void* obj, std::string_view name, TypedRef out) const;
		std::expected<void, FieldError> SetFieldValue(void* obj, std::string_view name, TypedRef in) const;

		std::expected<TypedRef, FieldError> GetFieldRef(void* obj, std::string_view name) const;
		std::expected<TypedRef, FieldError> GetFieldRef(const void* obj, std::string_view name) const;

		template <typename T>
		std::expected<T, FieldError> GetFieldAs(const void* obj, std::string_view name) const
		{
			const FieldInfo* field = FindFieldByName(name);
			if (!field)
				return std::unexpected(FieldError{FieldError::FieldNotFound});

			return field->GetAs<T>(obj);
		}

		template <typename T>
		std::expected<void, FieldError> SetFieldAs(void* obj, const std::string_view name, const T& value) const
		{
			const FieldInfo* field = FindFieldByName(name);
			if (!field)
				return std::unexpected(FieldError{FieldError::FieldNotFound});

			return field->SetAs<T>(obj, value);
		}

		template <typename T>
		std::expected<void, FieldError> MoveFieldAs(void* obj, const std::string_view name, T& value) const
		{
			const FieldInfo* field = FindFieldByName(name);
			if (!field)
				return std::unexpected(FieldError{FieldError::FieldNotFound});

			return field->MoveAs<T>(obj, value);
		}

		template <typename T>
		std::expected<std::reference_wrapper<T>, FieldError> GetFieldRefAs(void* obj, std::string_view name) const
		{
			const FieldInfo* field = FindFieldByName(name);
			if (!field)
				return std::unexpected(FieldError{FieldError::FieldNotFound});

			return field->GetRefAs<T>(obj);
		}

		template <typename T>
		std::expected<std::reference_wrapper<const T>, FieldError> GetFieldRefAs(const void* obj,
		                                                                         std::string_view name) const
		{
			const FieldInfo* field = FindFieldByName(name);
			if (!field)
				return std::unexpected(FieldError{FieldError::FieldNotFound});

			return field->GetRefAs<T>(obj);
		}

	private:
		std::string_view _displayName;
		std::span<const FieldInfo> _fields;
		std::span<const FuncInfo> _functions;
		std::string (*_stringifyThunk)(const void*) = nullptr;
	};
}
