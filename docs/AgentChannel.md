# Agent channel

**If you are an agent working on this engine, this is how you run the game and see what it does.**
Launch it, drive it, read its log, look at a screenshot. Do not launch the game and sleep hoping
something happened, and do not rely on timeouts to decide whether a change worked.

Development builds only. The whole feature is inside `#if defined(PGE_DEV)` and is compiled out of a
release build, so a shipped binary contains none of it. It is also opt-in: without the flag the
socket is never created.

## The loop

```bash
# 1. Launch, with the log going somewhere you can read.
./build/linux/PlaygroundGame/Debug/PlaygroundGame --agent-channel > /tmp/pge.log 2>&1 &

# 2. Wait for the channel, rather than sleeping a guessed number of seconds.
for i in $(seq 1 15); do [ -S /tmp/pge-agent.sock ] && break; sleep 1; done

# 3. Drive it. One invocation is one command.
scripts/pge PointerMoved 640 360
scripts/pge KeyPressed KeyA
scripts/pge KeyReleased KeyA

# 4. Perceive: a screenshot path, and the engine's own account of what happened.
scripts/pge screenshot          # prints a PNG path; read that file, it renders visually
grep -iE '\[error|\[warn|VUID' /tmp/pge.log

# 5. Quit cleanly. This exercises the real shutdown path.
scripts/pge CloseRequested
```

You have three independent views and should use them together: the shim's exit code, the engine log,
and the image. The log is the engine's side of the story, and it is the only place a failed capture
or a validation error shows up.

## Vocabulary

Verbs are `PlatformEventType` enumerators; code arguments are `InputCode` enumerators. They are
parsed with `EnumFromName`, so the protocol *is* the enum: renaming an enumerator renames the
command, and there is no second list to drift. Spelling is exact, `KeyF12` not `keyf12`.

```
PointerMoved <x> <y>                PointerMovedRelative <dx> <dy>
PointerScrolled <dx> <dy>           PointerButtonPressed <InputCode>
PointerButtonReleased <InputCode>   KeyPressed <InputCode> [repeat]
KeyReleased <InputCode>             CharacterTyped <codepoint>
FocusGained | FocusLost | CloseRequested
```

`WindowResized` is rejected: a size the window does not actually have buys nothing but a redundant
swapchain recreate.

## Screenshots have one entry point

The capture binding is F12, handled in `FrameCapture::ServiceRequests`, which `Engine::RunFrame`
calls once a frame before drawing. `scripts/pge screenshot` **injects that same key** and then waits for the new file; the engine has no capture command of its own. So an
agent taking a screenshot exercises exactly the path a person at the keyboard takes, and there is no
second path that could rot unnoticed.

Because the trigger is a key, the engine hands back no path. The shim finds the file by listing the
captures directory before injecting and waiting for a *new* entry that ends with a complete PNG
`IEND` chunk. Snapshotting first is what stops an earlier run's file being mistaken for this one,
and the `IEND` check is what stops a half-written file being read as a finished one.

Captures land in `<executable dir>/captures/capture-<timestamp>-<counter>.png`. Names are unique
across runs, not merely within one, for the same reason.

## Environment

| Variable | Meaning |
|---|---|
| `PGE_AGENT_SOCKET` | Socket path. Read by both the engine and the shim, so set it for both when running two instances. |
| `PGE_CAPTURES_DIR` | Where the shim looks for screenshots. Defaults to the Debug game's captures directory. |

## Shape, and what it is not

A reader thread accepts one connection at a time and parses one line per connection, staging events
into a buffer under a mutex. `Engine::RunFrame` drains that buffer into the `PlatformEventRecord`
immediately after the window server pump, so injected events join the same batch as real ones and no
consumer can tell which producer filled it. **The reader thread never touches the record**, which is
main-thread-only and cleared every pump.

Connect-per-command, not a session: an agent's tool calls are one-shot processes and cannot hold a
connection across them. Every command gets one reply line (`ok`, or `error <kind>`) and the
connection closes.

Deliberately absent: queries, property reads, entity inspection, request IDs, multiplexed clients,
and any framing beyond a newline. This is a development input channel, not the live-edit protocol.

## Known limits

- **One capture in flight.** The renderer holds a single pending capture; a second request in the
  same frame displaces the first with a warning. Back-to-back `screenshot` calls are fine because
  the shim waits for each file, but F12 pressed by hand in the same frame as an injected one loses
  a capture.
- **One instance per socket path.** A second game with the same path takes the name from the first,
  and whichever exits first unlinks the file. Give each instance its own `PGE_AGENT_SOCKET`.
- **A held connection blocks the next command** for up to five seconds, after which the reader drops
  it and carries on.
