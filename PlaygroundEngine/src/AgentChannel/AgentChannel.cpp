module;

#include <cerrno>
#include <cstring>
#include <poll.h>
#include <sys/eventfd.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#include "PlaygroundEngine/Log.h"

module PlaygroundEngine.AgentChannel;

#if defined(PGE_DEV)

import PlaygroundEngine.Log;
import PlaygroundEngine.Reflection;

namespace PgE
{
	namespace
	{
		// A request is one short line, so a client that never sends a newline is a client to drop
		// rather than a buffer to grow.

		constexpr std::size_t MaximumRequestLength = 1024;

		constexpr std::string_view ChannelFlag = "--agent-channel";

		bool HasFlag(const int argumentCount, char** arguments, const std::string_view flag)
		{
			for (int index = 1; index < argumentCount; ++index)
			{
				if (flag == arguments[index])
				{
					return true;
				}
			}

			return false;
		}

		// Shared with scripts/pge, which reads the same variable. A second instance would otherwise
		// take the default name from the first and leave it listening where no client can reach it.

		std::filesystem::path SocketPath()
		{
			if (const char* configured = std::getenv("PGE_AGENT_SOCKET"); configured != nullptr && *configured != '\0')
			{
				return configured;
			}

			return "/tmp/pge-agent.sock";
		}

		// A connected client that never sends its line would otherwise hold the single reader thread
		// forever, and every later command would queue behind it unanswered.

		constexpr int ConnectionTimeoutMilliseconds = 5000;

		enum class WaitOutcome : std::uint8_t
		{
			Ready,
			Stopped,
			TimedOut,
			Failed,
		};

		// The same clock the GLFW backend stamps its events with, so injected and real events order
		// against each other rather than sorting into two eras.

		std::uint64_t TimestampNow()
		{
			return static_cast<std::uint64_t>(
				std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
		}

		// Waits until the descriptor is readable or the channel is stopping. The wake descriptor is
		// what makes a blocking wait interruptible, so no accept or read is ever entered blind.

		WaitOutcome WaitReadable(const int descriptor, const int wakeDescriptor, const std::atomic<bool>& stopping, const int timeoutMilliseconds)
		{
			while (true)
			{
				if (stopping.load())
				{
					return WaitOutcome::Stopped;
				}

				std::array descriptors{pollfd{.fd = descriptor, .events = POLLIN, .revents = 0},
									   pollfd{.fd = wakeDescriptor, .events = POLLIN, .revents = 0}};

				const int ready = poll(descriptors.data(), descriptors.size(), timeoutMilliseconds);

				if (ready < 0)
				{
					if (errno == EINTR)
					{
						continue;
					}

					return WaitOutcome::Failed;
				}

				if (ready == 0)
				{
					return WaitOutcome::TimedOut;
				}

				if ((descriptors[1].revents & POLLIN) != 0 || stopping.load())
				{
					return WaitOutcome::Stopped;
				}

				if (descriptors[0].revents != 0)
				{
					return WaitOutcome::Ready;
				}
			}
		}

		// MSG_NOSIGNAL is not optional: a client that timed out and exited leaves a closed peer, and
		// the default SIGPIPE action would take the whole engine down while it answered.

		void SendReply(const int connectionDescriptor, const std::string_view reply)
		{
			std::size_t sent = 0;

			while (sent < reply.size())
			{
				const ssize_t written = send(connectionDescriptor, reply.data() + sent, reply.size() - sent, MSG_NOSIGNAL);

				if (written < 0)
				{
					if (errno == EINTR)
					{
						continue;
					}

					return;
				}

				sent += static_cast<std::size_t>(written);
			}
		}

		std::vector<std::string_view> Tokenize(const std::string_view line)
		{
			constexpr std::string_view separators = " \t\r\n";

			std::vector<std::string_view> tokens;
			std::size_t position = 0;

			while (position < line.size())
			{
				const std::size_t start = line.find_first_not_of(separators, position);
				if (start == std::string_view::npos)
				{
					break;
				}

				const std::size_t end = line.find_first_of(separators, start);
				const std::size_t stop = (end == std::string_view::npos) ? line.size() : end;

				tokens.push_back(line.substr(start, stop - start));
				position = stop;
			}

			return tokens;
		}

		std::optional<float> ParseFloat(const std::string_view text)
		{
			float value = 0.0f;

			if (const std::from_chars_result result = std::from_chars(text.data(), text.data() + text.size(), value);
				result.ec != std::errc{} || result.ptr != text.data() + text.size())
			{
				return std::nullopt;
			}

			// from_chars accepts "inf" and "nan". A NaN coordinate would poison every consumer that
			// integrates pointer position for the rest of the session, with no error to trace it to.

			if (!std::isfinite(value))
			{
				return std::nullopt;
			}

			return value;
		}

		std::optional<std::uint32_t> ParseUnsigned(const std::string_view text)
		{
			std::uint32_t value = 0;

			if (const std::from_chars_result result = std::from_chars(text.data(), text.data() + text.size(), value);
				result.ec != std::errc{} || result.ptr != text.data() + text.size())
			{
				return std::nullopt;
			}

			return value;
		}

		std::expected<PlatformEvent, AgentCommandError> ParseEvent(const PlatformEventType type, const std::span<const std::string_view> arguments)
		{
			switch (type)
			{
			case PlatformEventType::PointerMoved:
			case PlatformEventType::PointerMovedRelative:
			case PlatformEventType::PointerScrolled: {
				if (arguments.size() != 2)
				{
					return std::unexpected(AgentCommandError::BadArgumentCount);
				}

				const std::optional<float> x = ParseFloat(arguments[0]);
				const std::optional<float> y = ParseFloat(arguments[1]);

				if (!x || !y)
				{
					return std::unexpected(AgentCommandError::BadArgument);
				}

				return PlatformEvent{.Type = type, .X = *x, .Y = *y};
			}

			case PlatformEventType::KeyPressed:
			case PlatformEventType::KeyReleased: {
				if (arguments.empty() || arguments.size() > 2)
				{
					return std::unexpected(AgentCommandError::BadArgumentCount);
				}

				const std::optional<InputCode> code = EnumFromName<InputCode>(arguments[0]);

				if (!code || !IsKeyboardCode(*code))
				{
					return std::unexpected(AgentCommandError::BadArgument);
				}

				bool repeat = false;

				if (arguments.size() == 2)
				{
					if (type != PlatformEventType::KeyPressed || arguments[1] != "repeat")
					{
						return std::unexpected(AgentCommandError::BadArgument);
					}

					repeat = true;
				}

				return PlatformEvent{.Type = type, .Code = *code, .Repeat = repeat};
			}

			case PlatformEventType::PointerButtonPressed:
			case PlatformEventType::PointerButtonReleased: {
				if (arguments.size() != 1)
				{
					return std::unexpected(AgentCommandError::BadArgumentCount);
				}

				const std::optional<InputCode> code = EnumFromName<InputCode>(arguments[0]);

				if (!code || !IsPointerCode(*code))
				{
					return std::unexpected(AgentCommandError::BadArgument);
				}

				return PlatformEvent{.Type = type, .Code = *code};
			}

			case PlatformEventType::CharacterTyped: {
				if (arguments.size() != 1)
				{
					return std::unexpected(AgentCommandError::BadArgumentCount);
				}

				const std::optional<std::uint32_t> codepoint = ParseUnsigned(arguments[0]);

				if (!codepoint)
				{
					return std::unexpected(AgentCommandError::BadArgument);
				}

				return PlatformEvent{.Type = type, .Codepoint = static_cast<char32_t>(*codepoint)};
			}

			case PlatformEventType::FocusGained:
			case PlatformEventType::FocusLost:
			case PlatformEventType::CloseRequested: {
				if (!arguments.empty())
				{
					return std::unexpected(AgentCommandError::BadArgumentCount);
				}

				return PlatformEvent{.Type = type};
			}

			case PlatformEventType::WindowResized: {
				// Rejected rather than injected: a size the window does not actually have buys nothing
				// but a redundant swapchain recreate.

				return std::unexpected(AgentCommandError::UnknownCommand);
			}
			}

			return std::unexpected(AgentCommandError::UnknownCommand);
		}
	}

	std::expected<PlatformEvent, AgentCommandError> ParseCommand(const std::string_view line)
	{
		const std::vector<std::string_view> tokens = Tokenize(line);

		if (tokens.empty())
		{
			return std::unexpected(AgentCommandError::UnknownCommand);
		}

		const std::span arguments(tokens.begin() + 1, tokens.end());

		// The verb is the enumerator identifier itself, so the vocabulary is the enum rather than a
		// second list beside it that drifts.

		const std::optional<PlatformEventType> type = EnumFromName<PlatformEventType>(tokens[0]);

		if (!type)
		{
			return std::unexpected(AgentCommandError::UnknownCommand);
		}

		return ParseEvent(*type, arguments);
	}

	AgentChannel::AgentChannel(const int listenDescriptor, const int wakeDescriptor, std::filesystem::path socketPath)
		: _listenDescriptor(listenDescriptor), _wakeDescriptor(wakeDescriptor), _socketPath(std::move(socketPath))
	{}

	AgentChannel::~AgentChannel()
	{
		Stop();
	}

	std::expected<std::unique_ptr<AgentChannel>, AgentChannelError> AgentChannel::Start(std::filesystem::path socketPath)
	{
		const std::string nativePath = socketPath.native_encoded_string();

		sockaddr_un address{};
		address.sun_family = AF_UNIX;

		if (nativePath.size() >= sizeof(address.sun_path))
		{
			return std::unexpected(AgentChannelError::SocketPathTooLong);
		}

		std::memcpy(address.sun_path, nativePath.data(), nativePath.size());

		const int listenDescriptor = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);

		if (listenDescriptor < 0)
		{
			return std::unexpected(AgentChannelError::SocketCreationFailed);
		}

		// A socket file outlives the process that created it, so a previous run's leftover would fail
		// the bind with EADDRINUSE.

		unlink(nativePath.c_str());

		if (bind(listenDescriptor, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) != 0)
		{
			close(listenDescriptor);
			return std::unexpected(AgentChannelError::BindFailed);
		}

		// /tmp is world-writable, so without this any local user could inject input into the running
		// game. The channel is opt-in and development-only, but that is no reason to leave it open.

		if (chmod(nativePath.c_str(), S_IRUSR | S_IWUSR) != 0)
		{
			close(listenDescriptor);
			unlink(nativePath.c_str());
			return std::unexpected(AgentChannelError::BindFailed);
		}

		if (listen(listenDescriptor, 4) != 0)
		{
			close(listenDescriptor);
			unlink(nativePath.c_str());
			return std::unexpected(AgentChannelError::ListenFailed);
		}

		const int wakeDescriptor = eventfd(0, EFD_CLOEXEC);

		if (wakeDescriptor < 0)
		{
			close(listenDescriptor);
			unlink(nativePath.c_str());
			return std::unexpected(AgentChannelError::WakeDescriptorCreationFailed);
		}

		std::unique_ptr<AgentChannel> channel(new AgentChannel(listenDescriptor, wakeDescriptor, std::move(socketPath)));

		channel->_readerThread = std::thread(&AgentChannel::ReaderLoop, channel.get());

		return channel;
	}

	void AgentChannel::Stop()
	{
		if (!_readerThread.joinable())
		{
			return;
		}

		_stopping.store(true);

		// Waking the poll rather than closing the socket: closing a descriptor another thread is
		// blocked on races against that number being handed to something else.

		constexpr std::uint64_t wakeValue = 1;

		if (write(_wakeDescriptor, &wakeValue, sizeof(wakeValue)) < 0)
		{
			PGE_LOG(Warn, "Agent channel could not signal its reader thread to stop");
		}

		_readerThread.join();

		close(_listenDescriptor);
		close(_wakeDescriptor);
		unlink(_socketPath.native_encoded_string().c_str());
	}

	void AgentChannel::ReaderLoop()
	{
		while (!_stopping.load())
		{
			// No timeout on the accept: an idle channel is the normal state, unlike an idle connection.

			const WaitOutcome outcome = WaitReadable(_listenDescriptor, _wakeDescriptor, _stopping, -1);

			if (outcome == WaitOutcome::Failed)
			{
				PGE_LOG(Error, "Agent channel reader stopped: the listening socket became unusable");
				return;
			}

			if (outcome != WaitOutcome::Ready)
			{
				return;
			}

			const int connectionDescriptor = accept4(_listenDescriptor, nullptr, nullptr, SOCK_CLOEXEC);

			if (connectionDescriptor < 0)
			{
				if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)
				{
					continue;
				}

				PGE_LOG(Warn, "Agent channel stopped accepting connections");
				return;
			}

			HandleConnection(connectionDescriptor);

			close(connectionDescriptor);
		}
	}

	void AgentChannel::HandleConnection(const int connectionDescriptor)
	{
		std::string request;
		bool endOfInput = false;

		while (request.find('\n') == std::string::npos && request.size() < MaximumRequestLength && !endOfInput)
		{
			if (WaitReadable(connectionDescriptor, _wakeDescriptor, _stopping, ConnectionTimeoutMilliseconds) != WaitOutcome::Ready)
			{
				// A client that connects and then goes quiet otherwise presents as an unexplained
				// five-second stall before the next command is served.

				PGE_LOG(Trace, "Agent channel dropped a connection that sent no complete request: '{}'", request);
				return;
			}

			std::array<char, 256> buffer{};
			const ssize_t received = read(connectionDescriptor, buffer.data(), buffer.size());

			if (received < 0)
			{
				if (errno == EINTR)
				{
					continue;
				}

				return;
			}

			if (received == 0)
			{
				endOfInput = true;
				break;
			}

			request.append(buffer.data(), static_cast<std::size_t>(received));
		}

		const auto terminator = std::ranges::find(request, '\n');
		const std::string_view line(request.begin(), terminator);

		// Traced here rather than after the parse, so everything that arrives is logged: a rejected
		// verb, a truncated blob, and an accepted command all show up the same way.

		PGE_LOG(Trace, "Agent channel received: '{}'", line);

		// A request that hit the cap without a newline is truncated, and running a truncated command
		// would execute it against partial arguments rather than rejecting it.

		if (terminator == request.end() && !endOfInput)
		{
			PGE_LOG(Trace, "Agent channel rejected a request of {} bytes with no newline", request.size());
			SendReply(connectionDescriptor, std::format("error {}\n", EnumToName(AgentCommandError::BadArgument).value_or("Unknown")));
			return;
		}

		SendReply(connectionDescriptor, BuildResponse(line));
	}

	std::string AgentChannel::BuildResponse(const std::string_view line)
	{
		const std::expected<PlatformEvent, AgentCommandError> event = ParseCommand(line);

		if (!event)
		{
			return std::format("error {}\n", EnumToName(event.error()).value_or("Unknown"));
		}

		PlatformEvent stamped = *event;
		stamped.Timestamp = TimestampNow();

		const std::lock_guard guard(_mutex);
		_stagedEvents.push_back(stamped);

		return "ok\n";
	}

	void AgentChannel::DrainInto(PlatformEventRecord& record)
	{
		_drainBuffer.clear();

		{
			const std::lock_guard guard(_mutex);

			_drainBuffer.swap(_stagedEvents);
		}

		record.Append(_drainBuffer);
	}

	void AgentChannelHost::StartIfRequested(const int argumentCount, char** arguments)
	{
		// An arbitrary local input channel into a running process, so it stays opt-in even here, and
		// a channel that will not open is never a reason to fail the boot.

		if (!HasFlag(argumentCount, arguments, ChannelFlag))
		{
			return;
		}

		const std::filesystem::path socketPath = SocketPath();

		if (auto channel = AgentChannel::Start(socketPath); channel)
		{
			_channel = std::move(*channel);
			PGE_LOG(Info, "Agent channel listening on {}", socketPath.display_string());
		}
		else
		{
			PGE_LOG(Error, "Agent channel unavailable: {}", EnumToName(channel.error()).value_or("Unknown"));
		}
	}

	void AgentChannelHost::DrainInto(PlatformEventRecord& record)
	{
		if (_channel)
		{
			_channel->DrainInto(record);
		}
	}

	void AgentChannelHost::Stop()
	{
		_channel.reset();
	}
}

#else

namespace PgE
{
	// The release build of the whole feature: the root calls these three and nothing happens.

	void AgentChannelHost::StartIfRequested(int, char**)
	{}

	void AgentChannelHost::DrainInto(PlatformEventRecord&)
	{}

	void AgentChannelHost::Stop()
	{}
}

#endif
