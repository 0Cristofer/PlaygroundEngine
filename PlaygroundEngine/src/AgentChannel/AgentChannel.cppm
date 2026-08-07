export module PlaygroundEngine.AgentChannel;

import std;

import PlaygroundEngine.PlatformEvents;

namespace PgE
{
#if defined(PGE_DEV)

	export enum class AgentChannelError : std::uint8_t
	{
		SocketPathTooLong,
		SocketCreationFailed,
		WakeDescriptorCreationFailed,
		BindFailed,
		ListenFailed,
	};

	export enum class AgentCommandError : std::uint8_t
	{
		UnknownCommand,
		BadArgumentCount,
		BadArgument,
	};

	// Free of socket state on purpose: it is the one piece of this feature that is worth a test, and
	// it stays testable only while it is a pure string-to-event function. The timestamp is stamped by
	// the caller, so the same input always parses to the same event.

	export std::expected<PlatformEvent, AgentCommandError> ParseCommand(std::string_view line);

	/// Development-only local control endpoint. Lets an external process inject platform events over a
	/// unix socket, so an agent drives a running build the same way a person at the keyboard does. The
	/// public surface is main thread only; the reader thread reaches nothing but the staging buffer.
	export class AgentChannel
	{
	public:
		[[nodiscard]] static std::expected<std::unique_ptr<AgentChannel>, AgentChannelError> Start(std::filesystem::path socketPath);

		~AgentChannel();

		AgentChannel(const AgentChannel&) = delete;
		AgentChannel& operator=(const AgentChannel&) = delete;
		AgentChannel(AgentChannel&&) = delete;
		AgentChannel& operator=(AgentChannel&&) = delete;

		/// Hands everything the reader thread staged since the last call to the record. Called once per
		/// frame, right after the window server pump, so injected events join the same batch.
		void DrainInto(PlatformEventRecord& record);

	private:
		AgentChannel(int listenDescriptor, int wakeDescriptor, std::filesystem::path socketPath);

		void ReaderLoop();
		void HandleConnection(int connectionDescriptor);

		/// Parses one request, stages its effect, and returns the reply line. Reader thread only.
		std::string BuildResponse(std::string_view line);

		void Stop();

		int _listenDescriptor = -1;
		int _wakeDescriptor = -1;
		std::filesystem::path _socketPath;

		// Stop writes the wake descriptor rather than closing the socket: closing a descriptor another
		// thread is blocked on races against that number being reused.

		std::atomic<bool> _stopping{false};
		std::thread _readerThread;

		// Written by the reader thread, swapped out by DrainInto. The mutex covers this and nothing
		// else, so it is never held across a syscall or a record append.

		std::mutex _mutex;
		std::vector<PlatformEvent> _stagedEvents;

		std::vector<PlatformEvent> _drainBuffer;
	};

#endif

	/// The root's entire view of the channel, present in every build so no conditional compilation
	/// reaches the engine loop. Under PGE_RELEASE every method is an empty no-op and the channel it
	/// would own does not exist.
	export class AgentChannelHost
	{
	public:
		/// Opens the channel if the command line asks for it. Never fails the caller: an unavailable
		/// channel is logged and the engine runs without one.
		void StartIfRequested(int argumentCount, char** arguments);

		/// Hands over whatever arrived since the last call. Once per frame, right after the pump.
		void DrainInto(PlatformEventRecord& record);

		/// Joins the reader thread. Called before anything the channel could still reach goes away.
		void Stop();

	private:
#if defined(PGE_DEV)
		std::unique_ptr<AgentChannel> _channel;
#endif
	};
}
