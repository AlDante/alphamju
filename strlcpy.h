//
// Created by David Jenkins on 12.12.24.
//

// Define strlcpy for Linux and other operating systems without it.
// Windows uses strcpy_s

#ifndef BOOST_PREDEF_H
#include <boost/predef.h> // For identifying which OS we're compiling for
#endif

#if !defined(BOOST_OS_WINDOWS) && !defined(BOOST_OS_MACOS)
#ifndef STRLCPY_H
#define STRLCPY_H

#endif //STRLCPY_H
#endif
