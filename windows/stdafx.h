/*
 * Central include file to aid in the creation of a prebuilt header.
 */
#pragma once

// C4127: conditional expression is constant (triggered by GLib assertions)
// C4244: conversion with possible loss of data
// C4250: inheritance via dominance (all over sigc++ and gtkmm)
// C4505: unreferenced local function has been removed
// C4512: assignment operator could not be generated
#pragma warning(disable: 4127 4244 4250 4505 4512)

// The following macros define the minimum required platform.  The minimum
// required platform is the earliest version of Windows, Internet Explorer
// etc. that has the necessary features to run your application.  The macros
// work by enabling all features available on platform versions up to and 
// including the version specified.

// Modify the following defines if you have to target a platform prior to
// the ones specified below.  Refer to MSDN for the latest info on
// corresponding values for different platforms.

// Relax the OS version dependency to NT 5 (Windows 2000)
#ifndef WINVER
# define WINVER 0x0500
#endif
#ifndef _WIN32_WINNT
# define _WIN32_WINNT WINVER
#endif
#ifndef _WIN32_WINDOWS
# define _WIN32_WINDOWS 0x0410
#endif
#ifndef _WIN32_IE
# define _WIN32_IE 0x0501
#endif

// Enable strict type checking.  Also exclude some rarely-used definitions
// and poorly namespaced macros from the Windows headers.
#define STRICT 1
#define WIN32_LEAN_AND_MEAN 1
#define NOCOMM
#define NOIME
#define NOKANJI
#define NOMCX
#define NOMINMAX
#define NORPC
#define NOSERVICE
#define NOSOUND

// Hide deprecated API of the GTK+ libraries.  Note that this feature forces
// a compile-time error when currently used API is deprecated in the future.
#define GDKMM_DISABLE_DEPRECATED
#define GDK_DISABLE_DEPRECATED
#define GDK_MULTIHEAD_SAFE
#define GDK_PIXBUF_DISABLE_DEPRECATED
#define GLIBMM_DISABLE_DEPRECATED
#define GSF_DISABLE_DEPRECATED
#define GTKMM_DISABLE_DEPRECATED
#define GTK_DISABLE_DEPRECATED
#define G_DISABLE_DEPRECATED
#define LIBGLADE_DISABLE_DEPRECATED
#define LIBSIGC_DISABLE_DEPRECATED
#define PANGO_DISABLE_DEPRECATED
#define RSVG_DISABLE_DEPRECATED

// C4510: default constructor could not be generated (VC80 std::list)
// C4520: multiple default constructors specified (Gtk::PaperSize bug)
// C4610: object can never be instantiated (VC80 std::list)
#pragma warning(push)
#pragma warning(disable: 4510 4520 4610)

#include <sigc++/sigc++.h>
#include <glibmm.h>
#include <pangomm.h>
#include <atkmm.h>
#include <gdkmm.h>
#include <gtkmm.h>
#include <libglademm.h>

#include <vector>
#include <limits>
#include <list>
#include <functional>
#include <iterator>
#include <algorithm>
#include <sstream>
#include <stdexcept>
#include <string>
#include <locale>
#include <memory>
#include <new>
#include <numeric>
#include <iosfwd>
#include <iomanip>
#include <cfloat>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <cstring>
// Note: <iostream> should never be included from another header file,
// since it implies the instantiation of static global objects.

#pragma warning(pop)

#include <glib.h>
#include <glib-object.h>
#include <cairo.h>
#include <pango/pango.h>
#include <atk/atk.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>
#include <glade/glade.h>
#include <gdk/gdkgl.h>
#include <gtk/gtkgl.h>

#include <gdk/win32/gdkglwin32.h>
#include <gdk/glext/glext.h>
#include <GL/glu.h>

// By this time, the Windows header will very likely have been pulled in
// already by one of the numerous other header files above.  I put it here
// to avoid getting in the way of other code that might have to add some
// magic pre-include configuration defines of its own.
#include <windows.h>
#include <malloc.h>
#include <tchar.h>
#include <crtdbg.h>

// These should be safe to include even if we aren't actually building
// with SSE or SSE2 enabled.
#if defined(_M_X86) || defined(_M_X64)
# include <xmmintrin.h>
# include <emmintrin.h>
#endif
