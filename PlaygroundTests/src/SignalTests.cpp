#include <doctest/doctest.h>

import std;
import PlaygroundEngine.Signal;

TEST_CASE("Signal delivers its payload to every live subscriber")
{
	PgE::Signal<int> signal;

	int firstSeen = 0;
	int secondSeen = 0;

	const PgE::SignalSubscription firstSubscription = signal.Subscribe([&firstSeen](const int value) { firstSeen = value; });
	const PgE::SignalSubscription secondSubscription = signal.Subscribe([&secondSeen](const int value) { secondSeen = value; });

	signal.Emit(7);

	CHECK(firstSeen == 7);
	CHECK(secondSeen == 7);
	CHECK(signal.GetSubscriberCount() == 2);
}

TEST_CASE("Signal with no payload notifies its subscribers")
{
	PgE::Signal<> signal;

	int notificationCount = 0;
	const PgE::SignalSubscription subscription = signal.Subscribe([&notificationCount] { ++notificationCount; });

	signal.Emit();
	signal.Emit();

	CHECK(notificationCount == 2);
}

TEST_CASE("Dropping a subscription stops delivery")
{
	PgE::Signal<int> signal;

	int seen = 0;

	{
		const PgE::SignalSubscription subscription = signal.Subscribe([&seen](const int value) { seen = value; });
		signal.Emit(1);
	}

	signal.Emit(2);

	CHECK(seen == 1);
	CHECK(signal.GetSubscriberCount() == 0);
}

TEST_CASE("Resetting a subscription stops delivery")
{
	PgE::Signal<int> signal;

	int seen = 0;
	PgE::SignalSubscription subscription = signal.Subscribe([&seen](const int value) { seen = value; });

	CHECK(subscription.IsSubscribed());

	subscription.Reset();

	CHECK_FALSE(subscription.IsSubscribed());

	signal.Emit(3);

	CHECK(seen == 0);
}

TEST_CASE("A subscription outliving its signal unsubscribes from nothing")
{
	// The composition root tears systems down in reverse order, but a subscription surviving its
	// emitter must still be safe to destroy: the slot, not the subscriber, is what holds the
	// back-reference.

	PgE::SignalSubscription subscription;

	{
		PgE::Signal<int> signal;
		subscription = signal.Subscribe([](int) {});
	}

	CHECK(subscription.IsSubscribed());
	subscription.Reset();
}
