#pragma once

// Tenet version information and build-mode configuration.

#define TENET_VERSION_MAJOR 0
#define TENET_VERSION_MINOR 1
#define TENET_VERSION_PATCH 0

// Encoded as (major * 10000 + minor * 100 + patch), e.g. 0.1.0 -> 100.
#define TENET_VERSION \
    (TENET_VERSION_MAJOR * 10000 + TENET_VERSION_MINOR * 100 + TENET_VERSION_PATCH)

// 1 when used in header-only mode (defined by CMake option TENET_HEADER_ONLY).
#ifndef TENET_HEADER_ONLY
#define TENET_HEADER_ONLY 0
#endif

// In header-only mode every non-template function must be inline; in compiled
// mode the definitions live in the library's translation units instead.
#if TENET_HEADER_ONLY
#define TENET_INLINE inline
#else
#define TENET_INLINE
#endif
