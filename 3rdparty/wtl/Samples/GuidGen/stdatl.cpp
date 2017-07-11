// stdatl.cpp : source file that includes just the standard includes
//	GuidGen.pch will be the pre-compiled header
//	stdatl.obj will contain the pre-compiled type information

#include "stdatl.h"

#if (_ATL_VER < 0x0700) && !defined(_WIN32_WCE)
#include <atlimpl.cpp>
#endif //(_ATL_VER < 0x0700)
