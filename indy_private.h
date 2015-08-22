// inject_dylib, https://github.com/stephank/inject_dylib
// Copyright (c) 2015 Stéphan Kochen
// See the README.md for license details.

#ifndef indy_private_h
#define indy_private_h

#include "indy.h"

// Fairly arbitrary, but should be plenty for everything we do.
#define INDY_ALLOC_SIZE 16384

// Additional integer pointer types.
typedef uint32_t indy_uintptr_32;
typedef uint64_t indy_uintptr_64;

// Structures containing matchers for indy_symbols_x.
struct indy_link_symbol {
    const char *name;
    void *out;
};
struct indy_link_image {
    const char *name;   // Matches on LC_ID_DYLIB
    size_t num_symbols;
    struct indy_link_symbol *symbols;
};
struct indy_link {
    size_t num_images;
    struct indy_link_image *images;
};

// Symbol locators for 32-bit and 64-bit.
bool indy_symbols_32(mach_port_name_t task_port, struct indy_link *match, struct indy_error *err);
bool indy_symbols_64(mach_port_name_t task_port, struct indy_link *match, struct indy_error *err);

// Private structure filled by indy_inject before
// going into target architecture specific code.
struct indy_private {
    struct indy_info *info;
    void *local_region;
    mach_port_t target_port;
};

// Target architecture specific code.
bool indy_inject_i386(struct indy_private *p, struct indy_error *err);
bool indy_inject_x86_64(struct indy_private *p, struct indy_error *err);

// Inline error helpers.
static inline bool indy_set_error(struct indy_error *err, const char *descr, int64_t ret)
{
    if (err != NULL) {
        err->descr = descr;
        err->os_ret = ret;
    }
    return false;
}

static inline bool indy_set_error_ifneq(struct indy_error *err, const char *descr, int64_t ret, int64_t expected)
{
    bool ok = (ret == expected);
    if (!ok)
        indy_set_error(err, descr, ret);
    return ok;
}

#endif
