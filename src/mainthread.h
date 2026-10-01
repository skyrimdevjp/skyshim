#pragma once
#include <functional>

// Runs tasks on the game's main thread, once per frame, from a hook on the call to Main::Update in the game loop.
namespace skyui_compat::mainthread
{
	// Installs the per-frame hook. Verifies the call instruction first; returns false (and logs) if it does not match.
	bool Install(void (*a_log)(const char*, ...));

	// Queues a task; it runs on the main thread during the next frame.
	void Post(std::function<void()> a_task);

	// Queues a task that runs on the main thread after a_frames more frames.
	void PostAfterFrames(int a_frames, std::function<void()> a_task);
}
