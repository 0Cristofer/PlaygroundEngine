#pragma once

// =============================================================================
// Annotation Filtering + Type-Specific Function Registry
//
// Shows how to attach compile-time tags to fields and query them at reflection
// time, then builds a simple name→function-pointer map for a concrete type.
// This is the "simple" approach with a fixed signature — see generic_registry.h
// for the general case that works across any type and any signature.
//
// Concepts demonstrated:
//   [[=Tag{}]]                       — attaches a value as a compile-time annotation
//   std::meta::annotations_of_with_type(member, ^^Tag)
//                                    — retrieves annotations of a specific type on a member
//   +[](…){ … }                      — converts a captureless lambda to a plain function pointer
//   self.[:member:]()                — splicer used to call a reflected member function
// =============================================================================

// Empty tag struct. Attaching it as [[=Replicated{}]] on a field marks it
// for network replication. Any empty struct can serve as an annotation tag.
struct Replicated {};

struct PlayerState
{
    // [[=Replicated{}]] attaches a Replicated instance as a compile-time annotation.
    // Fields without the annotation are invisible to the replication system.
    [[=Replicated{}]] int health          = 100;
                      int internalCounter = 0;

    int Damage(int amount) { health -= amount; return health; }
    int Heal  (int amount) { health += amount; return health; }
};

// Builds a name → function-pointer map for PlayerState specifically.
// Every thunk has the same fixed signature (PlayerState&, int) → int,
// which keeps the code simple but limits it to this one type and signature.
using PlayerStateThunk = int (*)(PlayerState&, int);

std::unordered_map<std::string_view, PlayerStateThunk> BuildPlayerStateRegistry()
{
    std::unordered_map<std::string_view, PlayerStateThunk> registry;

    // members_of returns all members including functions.
    // access_context::unchecked() lets us see all members regardless of access specifier.
    template for (constexpr auto member :
        std::define_static_array(std::meta::members_of(^^PlayerState, std::meta::access_context::unchecked())))
    {
        // Skip constructors, destructors, copy/move operators — only keep user-defined functions.
        if constexpr (std::meta::is_function(member) && !std::meta::is_special_member_function(member))
        {
            // The unary + forces the lambda to decay to a plain function pointer.
            // self.[:member:](arg) splices the reflected function into a real member call.
            registry[std::meta::identifier_of(member)] =
                +[](PlayerState& self, int arg) { return self.[:member:](arg); };
        }
    }
    return registry;
}

void DemoAnnotations()
{
    std::cout << "\n=== Annotations ===\n";

    // Walk every data field of PlayerState and check for the Replicated annotation.
    template for (constexpr auto member :
        std::define_static_array(std::meta::nonstatic_data_members_of(^^PlayerState, std::meta::access_context::unchecked())))
    {
        // annotations_of_with_type returns all annotations of the given type on this member.
        // If the list is non-empty, the annotation is present.
        constexpr bool isReplicated = !std::meta::annotations_of_with_type(member, ^^Replicated).empty();

        std::cout << "  " << std::meta::identifier_of(member)
                  << " — replicated: " << std::boolalpha << isReplicated << "\n";
    }

    std::cout << "\n--- Type-specific function registry ---\n";
    PlayerState player;
    auto playerRegistry = BuildPlayerStateRegistry();
    std::cout << "  Damage(30) → " << playerRegistry.at("Damage")(player, 30) << "\n";
    std::cout << "  Heal(10)   → " << playerRegistry.at("Heal")  (player, 10) << "\n";
}
