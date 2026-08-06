export module PlaygroundEngine.WindowServer:BackendDeclarations;

namespace PgE
{
	// The platform backends, declared here and nowhere else so that both the classes that PIMPL to
	// them and the partitions that define them can name them. They cannot be declared in the
	// primary interface, because a backend partition may not import its own primary.

	// Consumers only ever see these incomplete names; the per-platform Backend partitions complete
	// them, and nothing about their definitions reaches this module's public surface.

	export class WindowServerBackend;
	export class WindowBackend;
}
