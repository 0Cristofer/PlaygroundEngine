#include <meta>

import std;

// Each header covers one aspect of C++26 reflection (P2996) as implemented in GCC 16.
// Include order matters: type_exploration.h uses PlayerState from annotation_registry.h.
#include "field_printing.h"       // ^^T, nonstatic_data_members_of, template for, splicers
#include "annotation_registry.h"  // [[=Tag{}]], annotations_of_with_type, type-specific registry
#include "generic_registry.h"     // parameters_of, return_type_of, any-erased generic registry
#include "type_exploration.h"     // bases_of, display_string_of, typeid+splice, enumerators_of
#include "serialization.h"        // FromJson<T>: JSON string → struct via field-name matching
#include "construction.h"         // feasibility: parameter annotations + constructor splicing

int main()
{
	DemoFieldPrinting();
	DemoAnnotations();
	DemoGenericRegistry();
	DemoTypeExploration();
	DemoSerialization();
	DemoConstruction();
	return 0;
}
