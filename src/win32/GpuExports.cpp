// These data exports are read directly by the NVIDIA Optimus and AMD
// PowerXpress/Enduro drivers at process creation time. They must be exported
// from the .exe itself (not from a DLL) and therefore live in a separate TU
// that is linked into the executable.
#include <windows.h>

extern "C" {

__declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;
__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;

} // extern "C"
