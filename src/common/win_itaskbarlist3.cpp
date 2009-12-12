#include "os.h"

#if defined(SYS_WINDOWS)

# include "common/win_itaskbarlist3.h"

GUID_EXT const GUID MTX_CLSID_TaskbarList GUID_SECT = { 0x56fdf344, 0xfd6d, 0x11d0, { 0x95, 0x8a, 0x0, 0x60, 0x97, 0xc9, 0xa0, 0x90 } };

#endif  // SYS_WINDOWS
