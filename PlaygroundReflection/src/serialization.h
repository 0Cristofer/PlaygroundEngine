#pragma once

// =============================================================================
// JSON Deserialization via Reflection
//
// Demonstrates the most direct serialization use-case: given a flat JSON string,
// populate any struct's fields by matching JSON keys to member names at compile
// time and converting the raw string values to the correct C++ types at runtime.
//
// This is the core value proposition of reflection for serialization:
// FromJson<T> works for any struct with no per-type boilerplate.
//
// Concepts demonstrated:
//   using FieldType = [:std::meta::type_of(member):]
//                          — splice the field's reflected type into a real C++ type alias
//   if constexpr (std::is_same_v<FieldType, int>)
//                          — compile-time branch per field type (no splicer in template args)
//   obj.[:member:] = value — splicer used for writing to a field, not just reading
// =============================================================================

// --- Demo struct ---

struct GameEntity
{
    std::string name   = "";
    int         health = 0;
    float       speed  = 0.f;
    bool        active = false;
};

// =============================================================================
// Minimal flat-JSON parser
//
// Handles only top-level key/value pairs — no nesting, no arrays.
// Returns a map of raw string values keyed by field name.
// String values have their quotes stripped; booleans and numbers are kept as-is.
// =============================================================================
std::map<std::string, std::string> ParseFlatJson(std::string_view json)
{
    std::map<std::string, std::string> fields;
    std::size_t pos = 0;

    auto skipWhitespace = [&]
    {
        while (pos < json.size() && std::isspace(static_cast<unsigned char>(json[pos])))
            ++pos;
    };

    // Reads a quoted string, returning the content without quotes.
    auto parseQuotedString = [&]() -> std::string
    {
        ++pos; // skip opening "
        std::size_t start = pos;
        while (pos < json.size() && json[pos] != '"') ++pos;
        std::string value{json.substr(start, pos - start)};
        ++pos; // skip closing "
        return value;
    };

    // Reads a value that is NOT a quoted string (number, bool, null).
    // Stops at the next comma or closing brace, trimming trailing whitespace.
    auto parseBareValue = [&]() -> std::string
    {
        std::size_t start = pos;
        while (pos < json.size() && json[pos] != ',' && json[pos] != '}') ++pos;
        std::size_t end = pos;
        while (end > start && std::isspace(static_cast<unsigned char>(json[end - 1]))) --end;
        return std::string{json.substr(start, end - start)};
    };

    skipWhitespace();
    if (pos < json.size() && json[pos] == '{') ++pos; // skip opening {

    while (pos < json.size())
    {
        skipWhitespace();
        if (pos >= json.size() || json[pos] == '}') break;
        if (json[pos] == ',') { ++pos; continue; }

        std::string key   = parseQuotedString();
        skipWhitespace();
        if (pos < json.size() && json[pos] == ':') ++pos;
        skipWhitespace();

        std::string value = (json[pos] == '"') ? parseQuotedString() : parseBareValue();
        fields[key] = std::move(value);
    }

    return fields;
}

// =============================================================================
// FromJson<T>
//
// Deserializes a flat JSON string into an instance of T.
//
// For each data field of T (enumerated at compile time via reflection):
//   1. Look up the field's name in the parsed JSON map.
//   2. Deduce the field's C++ type via type_of + splice into a using-alias.
//   3. Convert the raw string to that type and assign it through a splicer.
//
// Fields absent from the JSON are left at their default-initialised values.
// Field types not handled by the if-constexpr chain are silently skipped.
// =============================================================================
template <typename T>
T FromJson(std::string_view json)
{
    auto parsedFields = ParseFlatJson(json);
    T obj{};

    template for (constexpr auto member :
        std::define_static_array(std::meta::nonstatic_data_members_of(^^T, std::meta::access_context::unchecked())))
    {
        // Splice the field's reflected type into a real C++ type alias.
        // Using-alias RHS is not a template-argument position so this avoids -Wtemplate-body.
        using FieldType = [:std::meta::type_of(member):];

        auto it = parsedFields.find(std::string{std::meta::identifier_of(member)});
        if (it == parsedFields.end()) continue; // field not present in JSON → keep default

        const std::string& rawValue = it->second;

        // Compile-time branch per field type — exactly one branch is instantiated.
        // obj.[:member:] = … writes to the field via a member-access splicer.
        if constexpr (std::is_same_v<FieldType, int>)
            obj.[:member:] = std::stoi(rawValue);
        else if constexpr (std::is_same_v<FieldType, float>)
            obj.[:member:] = std::stof(rawValue);
        else if constexpr (std::is_same_v<FieldType, bool>)
            obj.[:member:] = (rawValue == "true");
        else if constexpr (std::is_same_v<FieldType, std::string>)
            obj.[:member:] = rawValue;
    }

    return obj;
}

void DemoSerialization()
{
    std::cout << "\n=== JSON Deserialization ===\n";

    // Hardcoded JSON — in a real engine this would come from a file or network packet.
    constexpr std::string_view json = R"({
    "name"  : "Player One",
    "health": 85,
    "speed" : 3.5,
    "active": true
})";

    auto entity = FromJson<GameEntity>(json);

    // Verify every field was populated correctly.
    std::cout << "  name   = " << entity.name                         << "\n";
    std::cout << "  health = " << entity.health                       << "\n";
    std::cout << "  speed  = " << entity.speed                        << "\n";
    std::cout << "  active = " << std::boolalpha << entity.active     << "\n";
}
