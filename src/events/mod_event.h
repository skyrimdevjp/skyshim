#pragma once
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace skyshim
{
	// Engine-independent ModEvent registry. Dispatch to the Papyrus VM is injected.
	struct Registration
	{
		std::uint64_t handle;  // Papyrus object handle
		std::string   callback;
		bool operator==(const Registration& o) const { return handle == o.handle && callback == o.callback; }
	};

	class ModEventRegistry
	{
	public:
		void Register(std::string a_event, std::uint64_t a_handle, std::string a_callback)
		{
			std::lock_guard l(_m);
			auto& v = _map[std::move(a_event)];
			Registration r{ a_handle, std::move(a_callback) };
			for (auto& e : v) {
				if (e.handle == r.handle) { e = r; return; }  // one callback per (event, object)
			}
			v.push_back(std::move(r));
		}

		void Unregister(const std::string& a_event, std::uint64_t a_handle)
		{
			std::lock_guard l(_m);
			auto it = _map.find(a_event);
			if (it == _map.end()) return;
			std::erase_if(it->second, [&](auto& e) { return e.handle == a_handle; });
		}

		std::vector<Registration> Snapshot(const std::string& a_event) const
		{
			std::lock_guard l(_m);
			auto it = _map.find(a_event);
			return it == _map.end() ? std::vector<Registration>{} : it->second;
		}

	private:
		mutable std::mutex _m;
		std::unordered_map<std::string, std::vector<Registration>> _map;
	};

	// skse.StoreIndices / LoadIndices backing store (session only).
	class IndexStore
	{
	public:
		void Store(const std::string& k, std::vector<std::int32_t> v) { std::lock_guard l(_m); _map[k] = std::move(v); }
		std::vector<std::int32_t> Load(const std::string& k) const
		{
			std::lock_guard l(_m);
			auto it = _map.find(k);
			return it == _map.end() ? std::vector<std::int32_t>{} : it->second;
		}

	private:
		mutable std::mutex _m;
		std::unordered_map<std::string, std::vector<std::int32_t>> _map;
	};
}
