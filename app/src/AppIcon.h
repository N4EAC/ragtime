#pragma once

#include <wx/iconbndl.h>

// Ragtime's icon at every size a window manager may ask for.
wxIconBundle appIcons();

#ifdef __WXOSX__
void setDockIcon();
#endif
