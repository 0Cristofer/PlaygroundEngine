module PlaygroundEngine.MeshCatalog;

import std;

namespace PgE
{
	MeshHandle MeshCatalog::Find(const std::string_view path) const
	{
		const auto entry = _handlesPerPath.find(path);

		return entry != _handlesPerPath.end() ? entry->second : MeshHandle{};
	}

	bool MeshCatalog::Contains(const std::string_view path) const
	{
		return _handlesPerPath.contains(path);
	}

	void MeshCatalog::Insert(std::string path, const MeshHandle handle)
	{
		_handlesPerPath.insert_or_assign(std::move(path), handle);
	}
}
