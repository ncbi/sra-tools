#ifndef __SHARQ_VERSION_H__
#define __SHARQ_VERSION_H__

/**
 * @file version.h
 * @brief SharQ versioning
 *
 */

// VDB-4764: use the toolkit-wide version

// #define SHARQ_VERSION_MAJOR 1
// #define SHARQ_VERSION_MINOR 0
// #define SHARQ_VERSION_PATCH 3 /// Doxygen comments; defline_matcher groupSize refectoring

// #define SHARQ_VERSION to_string(SHARQ_VERSION_MAJOR) + "." + to_string(SHARQ_VERSION_MINOR) + "." + to_string(SHARQ_VERSION_PATCH)
#include <shared/toolkit.vers.h>
#define SHARQ_VERSION ( to_string(TOOLKIT_VERS >> 24) + "." + to_string((TOOLKIT_VERS >> 16) & 0x00ff) + "." + to_string(TOOLKIT_VERS & 0xffff) )

#endif
