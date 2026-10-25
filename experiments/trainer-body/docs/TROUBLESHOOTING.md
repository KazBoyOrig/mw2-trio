# Required Steam API library

MW2-Trio does not include, download, generate or repair `steam_api.dll`. You supply a complete working game installation. A matching executable hash does not certify third-party libraries as safe.

Before preparing a full copy, the launcher reads the required dependency. A missing or unreadable file encountered at that preflight stops preparation before the destination is created. Windows security errors225/226 receive an explicit message. Other access failures are reported as access failures; they are not automatically labelled antivirus detections. A file can still change or disappear after preflight; an incomplete copy remains unmarked and ineligible for launch.

Before a session, the launcher also compares this library's full SHA-256 with its prepared-copy inventory. A same-size replacement is detected. A matching hash establishes consistency with that inventory, not a safety verdict or proof of licensing.

If security software blocks or quarantines the file, review the detection and repair the installation from a trusted source. The launcher does not restore quarantined files, change exclusions or modify protection settings. Do not use a randomly downloaded DLL as a replacement.

For a legitimately owned Steam installation, use [Steam's file verification procedure](https://help.steampowered.com/en/faqs/view/0C48-FCBD-DA71-93EB). If you believe a detection is wrong, use [Microsoft's official submission portal](https://www.microsoft.com/en-us/wdsi/filesubmission/) for assessment. No false-positive conclusion follows merely from the game otherwise running or a library exporting the expected functions.

[Microsoft's Windows error documentation](https://learn.microsoft.com/en-us/windows/win32/debug/system-error-codes--0-499-) defines225 as a virus/unwanted-software block and226 as a corresponding removal. A launcher error cannot establish the detailed behavior of the detected file.
