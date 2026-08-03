export module PlaygroundEngine.Signal;

import std;

namespace PgE
{
	// RAII handle for one subscription. Holding it keeps the handler registered; dropping or
	// resetting it unregisters. Move-only, so a live subscription always has exactly one owner.

	export class SignalSubscription
	{
	public:
		SignalSubscription() = default;

		SignalSubscription(const SignalSubscription&) = delete;
		SignalSubscription& operator=(const SignalSubscription&) = delete;

		SignalSubscription(SignalSubscription&&) noexcept = default;
		SignalSubscription& operator=(SignalSubscription&&) noexcept = default;

		~SignalSubscription() = default;

		void Reset()
		{
			_token.reset();
		}

		[[nodiscard]] bool IsSubscribed() const
		{
			return _token != nullptr;
		}

	private:
		template <typename... Payload>
		friend class Signal;

		explicit SignalSubscription(std::shared_ptr<const void> token) : _token(std::move(token))
		{}

		std::shared_ptr<const void> _token;
	};

	// A typed notification owned by the system that emits it. Subscribed at wiring time in the
	// composition root, fired only from a defined drain point in the frame. There is no global bus:
	// a subscriber names the signal it wants, so the dependency stays visible at the wiring site.

	export template <typename... Payload>
	class Signal
	{
	public:
		[[nodiscard]] SignalSubscription Subscribe(std::function<void(Payload...)> handler)
		{
			// The token is what the subscriber owns; the slot only observes it. That inverts the
			// usual lifetime hazard: a subscriber outliving the signal finds nothing to unregister
			// from, and a signal outliving a subscriber drops the slot on its next pass.

			std::shared_ptr<const void> token = std::make_shared<const std::byte>();

			DropExpiredSlots();
			_slots.push_back(Slot{.Token = token, .Handler = std::move(handler)});

			return SignalSubscription(std::move(token));
		}

		void Emit(const Payload&... payload)
		{
			DropExpiredSlots();

			for (std::size_t slotIndex = 0; slotIndex < _slots.size(); ++slotIndex)
			{
				if (const std::shared_ptr<const void> alive = _slots[slotIndex].Token.lock())
				{
					_slots[slotIndex].Handler(payload...);
				}
			}
		}

		[[nodiscard]] std::size_t GetSubscriberCount() const
		{
			return static_cast<std::size_t>(std::ranges::count_if(_slots, [](const Slot& slot) { return !slot.Token.expired(); }));
		}

	private:
		struct Slot
		{
			std::weak_ptr<const void> Token;
			std::function<void(Payload...)> Handler;
		};

		void DropExpiredSlots()
		{
			std::erase_if(_slots, [](const Slot& slot) { return slot.Token.expired(); });
		}

		std::vector<Slot> _slots;
	};
}
