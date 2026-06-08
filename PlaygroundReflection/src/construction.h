#pragma once

// =============================================================================
// Construction via Annotated Static Factory
//
// Problem: constructor splicing is not supported in GCC 16 (or P2996 generally
// — constructors cannot appear in a splice expression). Parameter annotations
// ARE supported on both constructors and regular functions.
//
// Solution: a [[=Factory{}]]-annotated static function whose parameters are
// tagged [[=Injected{}]] or [[=Serialized{}]]. ConstructFromJson<T> finds this
// function at compile time and generates the call automatically — no separate
// code generation tool, std::meta IS the code generation.
//
// Concepts demonstrated:
//   consteval function + regular for loop  — compile-time search over members
//   FindFactory<T>() as a template arg     — passing a consteval result directly
//                                            into a template parameter (std::meta::info
//                                            is a structural type, so this is legal)
//   InvokeStatic<Fn>                       — static-function variant of InvokeImpl
//                                            (no object pointer, direct [:Fn:] call)
//   mixed injected/serialized dispatch     — runtime counter tracks which injected
//                                            arg to pull next as template for unrolls
// =============================================================================

struct Factory    {};   // marks the static function to use for construction
struct Injected   {};   // parameter is a runtime dependency, passed by the caller
struct Serialized {};   // parameter comes from serialized data (JSON, save file, …)

// -----------------------------------------------------------------------------
// Demo type — no default constructor.
// Weapon::Create is the single, meaningful construction point.
// -----------------------------------------------------------------------------
struct Weapon
{
    // [[=Factory{}]] tells the reflection system this is the construction entry point.
    // [[=Injected{}]] / [[=Serialized{}]] tell it where each argument comes from.
    [[=Factory{}]]
    static Weapon Create([[=Injected{}]]  int         id,
                         [[=Serialized{}]] std::string name,
                         [[=Serialized{}]] float       damage)
    {
        return Weapon(id, std::move(name), damage);
    }

    int         id     = 0;
    std::string name;
    float       damage = 0.f;

private:
    Weapon(int id, std::string name, float damage)
        : id(id), name(std::move(name)), damage(damage) {}
};

// -----------------------------------------------------------------------------
// Step 1: FindFactory<T>()
//
// Searches T's members at compile time for the [[=Factory{}]]-annotated function.
// A consteval function can use a plain for loop — template for is only needed
// when you want to expand each iteration as a separate template instantiation.
// Returns std::meta::info — a structural scalar type, usable as a template arg.
// -----------------------------------------------------------------------------
template <typename T>
consteval std::meta::info FindFactory()
{
    for (std::meta::info member : std::meta::members_of(^^T, std::meta::access_context::unchecked()))
        if (std::meta::is_function(member) && !std::meta::is_special_member_function(member))
            if (!std::meta::annotations_of_with_type(member, ^^Factory).empty())
                return member;

    // No factory found — downstream template instantiation will produce a clearer error.
    return {};
}

// -----------------------------------------------------------------------------
// Step 2: InvokeStatic<Fn> — static-function variant of InvokeImpl
//
// For a static member function, [:Fn:] splices directly to a callable
// (equivalent to Weapon::Create) so no object pointer is needed.
// The InvokeImpl/Invoke split for the same consteval-only reason as before.
// -----------------------------------------------------------------------------
template <std::meta::info Fn, std::size_t... I>
std::any InvokeStaticImpl(Args args, std::index_sequence<I...>)
{
    constexpr auto params = std::define_static_array(std::meta::parameters_of(Fn));
    using ReturnType = [:std::meta::return_type_of(Fn):];

    if constexpr (std::is_void_v<ReturnType>)
    {
        [:Fn:](ArgCast<params[I]>::from(args[I])...);
        return {};
    }
    else
    {
        return [:Fn:](ArgCast<params[I]>::from(args[I])...);
    }
}

template <std::meta::info Fn>
std::any InvokeStatic(Args args)
{
    constexpr std::size_t paramCount = std::meta::parameters_of(Fn).size();
    return InvokeStaticImpl<Fn>(args, std::make_index_sequence<paramCount>{});
}

// -----------------------------------------------------------------------------
// Step 3: ConstructImpl<T, Fn>
//
// This is the generated code — what a separate code generator would emit.
// With std::meta it is written once generically and instantiated per type.
//
// Walks Fn's parameter list in declaration order. For each parameter:
//   Injected   → pulls the next value from injectedArgs (caller-provided)
//   Serialized → looks up by parameter name in parsedJson, converts by type
//
// Assembles a std::vector<std::any> in the correct order, then forwards to
// InvokeStatic which unpacks and calls the factory with real types.
// -----------------------------------------------------------------------------
template <typename T, std::meta::info Fn>
T ConstructImpl(const std::map<std::string, std::string>& parsedJson, Args injectedArgs)
{
    std::vector<std::any> allArgs;
    int injectedIdx = 0;   // runtime counter — incremented in Injected branches as
                           // template for unrolls, preserving declaration order

    template for (constexpr auto param :
        std::define_static_array(std::meta::parameters_of(Fn)))
    {
        using ParamType = [:std::meta::type_of(param):];

        if constexpr (!std::meta::annotations_of_with_type(param, ^^Injected).empty())
        {
            // Injected: take the next runtime dependency from the caller's list.
            allArgs.push_back(injectedArgs[injectedIdx++]);
        }
        else
        {
            // Serialized: look up by the parameter's own name, convert to its type.
            // This is the compile-time→runtime bridge: identifier_of gives the key,
            // type_of drives the conversion branch.
            const std::string& raw = parsedJson.at(std::string{std::meta::identifier_of(param)});

            if constexpr (std::is_same_v<ParamType, int>)
                allArgs.push_back(std::stoi(raw));
            else if constexpr (std::is_same_v<ParamType, float>)
                allArgs.push_back(std::stof(raw));
            else if constexpr (std::is_same_v<ParamType, std::string>)
                allArgs.push_back(raw);
        }
    }

    return std::any_cast<T>(InvokeStatic<Fn>(allArgs));
}

// -----------------------------------------------------------------------------
// Public API
//
// FindFactory<T>() is called at compile time and its result is passed directly
// as a non-type template argument to ConstructImpl — no runtime lookup.
// -----------------------------------------------------------------------------
template <typename T>
T ConstructFromJson(std::string_view json, Args injectedArgs = {})
{
    auto parsedJson = ParseFlatJson(json);
    return ConstructImpl<T, FindFactory<T>()>(parsedJson, injectedArgs);
}

// -----------------------------------------------------------------------------
// Demo
// -----------------------------------------------------------------------------
void DemoConstruction()
{
    std::cout << "\n=== Construction via Annotated Factory ===\n";

    // 'id' is Injected — provided by the caller (assigned by the entity system).
    // 'name' and 'damage' are Serialized — come from the JSON.
    // The constructor is private; Create() is the only entry point.
    constexpr std::string_view json = R"({
        "name"  : "Excalibur",
        "damage": 42.5
    })";

    std::vector<std::any> injectedArgs = {1001};   // runtime id
    auto weapon = ConstructFromJson<Weapon>(json, injectedArgs);

    std::cout << "  id     = " << weapon.id     << "\n";
    std::cout << "  name   = " << weapon.name   << "\n";
    std::cout << "  damage = " << weapon.damage  << "\n";
}
