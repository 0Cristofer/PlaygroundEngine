export module PlaygroundEngine.PlatformEvents:PlatformEventRecord;

import std;

import :PlatformEvent;

namespace PgE
{
	/// Owns the master list of platform events that happened in this frame. Events are completely unprocessed and flat.
	export class PlatformEventRecord
	{
	public:
		static constexpr std::size_t DefaultReservedCapacity = 256;

		explicit PlatformEventRecord(const std::size_t reservedCapacity = DefaultReservedCapacity)
		{
			_events.reserve(reservedCapacity);
		}

		void Clear()
		{
			_events.clear();
		}

		void Append(const PlatformEvent& event)
		{
			_events.push_back(event);
		}

		/// Appends a whole batch, preserving its order. What a producer that cannot write straight
		/// into this record uses to hand over what it accumulated.
		void Append(const std::span<const PlatformEvent> events)
		{
			_events.insert(_events.end(), events.begin(), events.end());
		}

		[[nodiscard]] std::span<const PlatformEvent> GetEvents() const
		{
			return _events;
		}

		[[nodiscard]] bool IsEmpty() const
		{
			return _events.empty();
		}

		[[nodiscard]] bool HasEvent(const PlatformEventType type) const
		{
			return std::ranges::any_of(_events, [type](const PlatformEvent& event) { return event.Type == type; });
		}

	private:
		std::vector<PlatformEvent> _events;
	};
}
