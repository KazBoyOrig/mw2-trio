#pragma once
#include <windows.h>

/* Called at DLL_PROCESS_ATTACH. A regular opt-in file is required before a
   refcount-protected worker is scheduled; engine work occurs after DllMain. */
void control184_schedule(HINSTANCE instance, DWORD initial_thread_id);
