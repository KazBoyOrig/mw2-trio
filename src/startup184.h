#pragma once
#include <windows.h>

/* DLL_PROCESS_ATTACH only. Returns 1 if the exact static-startup CALL is hooked,
   0 for disabled/ordinary refusal, -1 for uncertain rollback (fail static load).
   Storage installation itself runs later in the WinMain wrapper, outside DllMain. */
int startup184_attach(HMODULE module, LPVOID reserved, DWORD initial_thread_id);
