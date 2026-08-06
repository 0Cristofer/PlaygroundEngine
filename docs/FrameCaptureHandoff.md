# Frame capture: session handoff

Ephemeral. Delete once the agent channel lands and the capture path has a real trigger. This file carries only what the code does not: process state, resumption order, and the reasoning behind decisions that would otherwise get re-derived.

## State

Frame capture is **implemented and validated**, on branch `feature/frame-capture` in the `PGE-frame-capture` worktree, based on local `main` (20e01b8). Nothing is committed. Fourteen files: 13 modified, plus the new `PlaygroundTests/src/FilesTests.cpp`.

What exists:

- `RendererVulkan::RequestCapture(path)` queues a capture; `DrawFrame` services it between the submit and the present.
- `ReadImageToHost` in `VulkanUtils`, copying a colour image into a host-visible buffer.
- `EncodeImagePng` in `PlaygroundEngine.Image`, backed by `stb_image_write` in the existing `StbImage` target.
- `WriteBinaryFile` in `PlaygroundEngine.Files`.
- `SwapChainResources::SupportsTransferSource`, so the capability is reported rather than required.

`scripts/verify.sh` is green on `build`, `format`, `lint`, and `test` (200 tests). The `matrix`, `sanitizers`, and `coverage` stages have **not** been run.

Validated by running `PlaygroundGame` under WSLg three times and reading the resulting PNGs back: correct scene, correct channel order, zero validation errors, zero VUID hits. `/code-review` was run, returned six findings, and all six are fixed.

**There is no trigger.** `RequestCapture` has no caller in the tree. Every validation used a temporary frame-counter scaffold in `Engine::RunFrame` that was removed afterwards, so `Engine.cpp` is untouched in the diff.

## Next step

**1. Wire the trigger. Blocked on the window server merge.**

`main` has no `PlatformEventRecord`, which is why this branch ships without a trigger. Once `feature/window-server` merges, `Engine::RunFrame` already reads the batch for resize and close; capture joins that block:

```
if (_rendererVulkan && _platformEvents.HasEvent(PlatformEventType::KeyPressed /* F12 */))
{
    _rendererVulkan->RequestCapture(/* path */);
}
```

Note the record carries no "which key" helper yet, only `HasEvent(type)`. Reading a specific `InputCode` out of the batch means iterating `GetEvents()`. That is fine, and it is the first real consumer that wants a slightly richer query, so resist adding one until a second consumer asks.

**2. The agent channel.** The reason this work exists. Design agreed in session, unbuilt:

- A `AF_UNIX` listener, opened at boot, guarded by `PGE_DEV` and off unless a flag turns it on.
- A reader thread doing blocking accept and read, parsing one event per line.
- A `std::mutex` plus staging `std::vector<PlatformEvent>` hand-off.
- A drain in `Engine::RunFrame` immediately after `_windowServer->Pump(_platformEvents)`, appending into the same record.
- A `scripts/pge` shim so an agent issues `scripts/pge pointer_move 200 300` rather than speaking a socket.

The `screenshot` command returns a **file path**, not image bytes: an agent's file-reading tool renders PNGs visually, so path plus read is the whole perceive half of the loop. Have the shim poll briefly for the file so it stays one call.

## Settled, do not re-litigate

| Decision | Short reason |
|---|---|
| Capture is a renderer command, not a key the renderer watches | Which input means "capture" is root policy; the agent channel has no keys to press |
| Readback destination is a buffer, not a second image | A buffer copy is tightly packed by definition; an image carries an implementation-defined row pitch |
| Serviced after the submit, before the present | The only window where the frame is finished and the image is still application-owned |
| A device wait per capture is acceptable | Zero-stall needs double-buffered readback plus a frame of latency, for a path that runs on request |
| Capture failure never fails the frame | A screenshot must not end the run; the caller learns the outcome from the file |
| `eTransferSrc` is opportunistic, not required | A debug-only capability must never veto rendering or a resize |
| Swapchain format resolved before the readback | The readback assumes four bytes per pixel, so a wider format must be rejected before a copy is issued |
| Alpha forced opaque | `compositeAlpha` is `eOpaque`, so nothing constrained that channel; honouring it can yield an invisible PNG |
| No gamma step | An sRGB swapchain holds sRGB-encoded bytes, which is exactly what a PNG stores |
| Encode returns bytes, `Files` writes them | Symmetric with `DecodeImage`; one I/O path, one file-error type |
| The event vocabulary comes from `EnumFromName` | The reflection enumeration facet already gives name to value, so the text protocol tracks the enums instead of drifting beside them |
| Connect-per-command, not a session | Agent tool calls are one-shot processes; a connection cannot span them |

## Open

- **Capture outcome reporting.** Today: a log line, and the caller checks for the file. The agent channel may want a real result, which would mean something like `TakeCaptureResult()`. Deliberately not built for a consumer that does not exist. Decide when the channel does.
- **Who chooses the path.** The caller currently supplies a full path. An agent asking for "a screenshot" with no path needs a naming policy and a directory, which is root policy, not renderer policy.
- **One pending capture only.** A second request before the next frame displaces the first, with a warning. If an agent ever wants burst captures, this becomes a queue.
- **Non-32-bit swapchain formats are rejected.** Only the `B8G8R8A8` and `R8G8B8A8` families are handled. Anything else needs a real format conversion table.
- **`RendererCreationErrorKind` now carries capture and operational kinds.** Reusing it beat inventing a parallel taxonomy for one feature, but the name is less accurate than it was. Worth a rename, not in this diff.
- **Which window to capture**, once a second window exists. Ties into the record's deferred window identity field.

## Corrections made during this session

Recorded so they are not re-derived.

- **`eTransferSrc` was first made a hard swapchain creation requirement.** Wrong: it made a debug feature able to stop the engine booting, and to kill a running instance on its first resize, on any surface offering only the specification's guaranteed `eColorAttachment`. Capabilities that serve one optional feature get reported, not required.
- **The format check originally ran after the readback.** `ReadImageToHost` sizes its buffer at four bytes per pixel, so an exotic format was copied against a wrong size before being rejected. Order matters when a helper's contract is an assumption rather than a parameter.
- **`waitIdle` failure originally propagated out of `DrawFrame`,** directly contradicting the "fire and forget" contract written three lines above it in the header, and abandoning an acquired swapchain image before its present. A documented contract is worth re-reading against the code that implements it.
- **The PNG dimension guard first bounded each axis independently.** That covers only the row stride. `stb_image_write.h:1144` allocates `(x * n + 1) * y` in `int` arithmetic, so the product is what has to be bounded; 65536 by 16384 passed the per-axis guard and overflowed inside stb. Verified in the stb source, not assumed.
- **`std::filesystem::path::string()` is deprecated in GCC 16** and is a hard error under this project's `-Werror`. Use `display_string()` for logs, `native_encoded_string()` for OS APIs.
- **Filtering build output through `grep -E "error:"` hides CMake's own failures,** which say "CMake Error". Combined with a stray `cd`, this meant a stale binary was re-run several times while its new logging never appeared. Check the artefact timestamp when output disagrees with the source.
- **An apparent hang was self-inflicted.** Leaked `PlaygroundGame` processes from failed backgrounding were pinning the software rasterizer at load average 30. `vkcube` running fine is what separated "my code" from "this machine"; keep a known-good Vulkan client around as the control.

## Working context

- **Pair mode.** The user drives and takes the final stance at each fork. Surface trade-offs with a recommendation rather than a survey.
- **`/code-review` is user-triggered** and cannot be launched from a session. Ask for it at step 5 rather than substituting a self-review, which the working method explicitly calls insufficient.
- **The capture path has no automated test and cannot easily have one**, since it needs a device, a surface, and a swapchain. It is validated by running the game and looking at the image. Budget for that on any change to the readback, the format handling, or the barriers.
