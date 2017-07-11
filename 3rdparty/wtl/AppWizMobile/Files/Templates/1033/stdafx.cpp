// stdafx.cpp : source file that includes just the standard includes
//	[!output PROJECT_NAME].pch will be the pre-compiled header
//	stdafx.obj will contain the pre-compiled type information

#include "stdafx.h"
[!if WTL_ENABLE_AX && WTL_EVC_COMPAT]

#if (_ATL_VER < 0x0700)
#include <wceatl.cpp>
#endif //(_ATL_VER < 0x0700)
[!endif]
