#ifndef __SHARQ_VERSION_H__
#define __SHARQ_VERSION_H__

/**
 * @file version.h
 * @brief SharQ versioning
 * 
 */

#define SHARQ_VERSION_MAJOR 1
#define SHARQ_VERSION_MINOR 2 /// increased error tolerance
#define SHARQ_VERSION_PATCH 0 

#define SHARQ_VERSION to_string(SHARQ_VERSION_MAJOR) + "." + to_string(SHARQ_VERSION_MINOR) + "." + to_string(SHARQ_VERSION_PATCH)

#endif

