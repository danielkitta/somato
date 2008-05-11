/*
 * Copyright (c) 2004-2006  Daniel Elstner  <daniel.kitta@gmail.com>
 *
 * This file is part of Somato.
 *
 * Somato is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Somato is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Somato; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#pragma once

#define PACKAGE_NAME      "Somato"
#define PACKAGE_TARNAME   "somato"
#define PACKAGE_VERSION   "0.6"
#define PACKAGE_STRING    PACKAGE_NAME " " PACKAGE_VERSION
#define PACKAGE_BUGREPORT "daniel.kitta@gmail.com"

// XXX: temporary HACK
#define SOMATO_PKGDATADIR "C:\\Users\\Daniel\\Documents\\Visual Studio 2008\\Projects\\Somato\\ui"

#if _MSC_VER >= 1400
// Indicate support for SSE if enabled in the build configuration.
# if (_M_IX86_FP >= 1) || defined(_M_X64)
#  define SOMATO_VECTOR_USE_SSE 1
# endif
# if (_M_IX86_FP >= 2) || defined(_M_X64)
#  define SOMATO_VECTOR_USE_SSE2 1
# endif
#endif /* _MSC_VER >= 1400 */

/*
 * These two functions are not supported and probably will never be.
 * Our default implementation using _mm_alloc() works nicely enough,
 * obviating the need for the first one.  sincosf() on the other hand
 * simply isn't defined in the C runtime in any form whatsoever, not
 * even as compiler intrinsic.  However, the code where it would have
 * been used is hardly a critical path anyway.
 *
 * #undef SOMATO_HAVE_POSIX_MEMALIGN
 * #undef SOMATO_HAVE_SINCOSF
 */

/*
 * On Windows with MS Visual C++, we use our own custom Util::RawVector<T>
 * template class instead of std::vector<T> in some cases.  This turned out
 * to be necessary for mainly these two reasons:  Microsoft's std::vector<>
 * cannot be used with any element type which imposes alignment requirements
 * larger than the target platform's default.  This restriction is somewhat
 * accidental, and wouldn't be there if std::vector<T>::resize() and maybe
 * a few other methods had been declared with constant reference arguments.
 *
 * Please refer to "rawvector.h" for the class description and source code.
 */
#define SOMATO_USE_RAWVECTOR 1
/*
 * Include this Windows-specific header file here, so we won't have to
 * sprinkle the code with preprocessor conditionals all over the place.
 */
#include "rawvector.h"
