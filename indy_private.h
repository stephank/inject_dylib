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
struct indy_link_symbol
{
    const char *name;
    void *out;
};

struct indy_link_image
{
    const char *name;   // Matches on LC_ID_DYLIB
    size_t num_symbols;
    struct indy_link_symbol *symbols;
};

struct indy_link
{
    size_t num_images;
    struct indy_link_image *images;
};

// Private structure filled by indy_inject before
// going into target architecture specific code.
struct indy_private
{
    const struct indy_info *info;
    struct indy_result *res;

    char *dylib_token;

    mach_port_t task;

    mach_vm_address_t region_addr;
    mach_vm_size_t region_size;
    void *local_region;

    thread_act_t target_thread;

    mach_vm_address_t exit_status_addr;
};

// 32-bit and 64-bit Mach-O symbol locators.
bool indy_symbols_32(struct indy_private *p, struct indy_link *match);
bool indy_symbols_64(struct indy_private *p, struct indy_link *match);

// Setup the info structure in the target and thread state.
bool indy_setup_i386(struct indy_private *p);
bool indy_setup_x86_64(struct indy_private *p);

// Inline error helpers.
static inline bool indy_set_error(struct indy_result *res, const char *error, int64_t os_value)
{
    if (res != NULL) {
        res->error = error;
        res->os_value = os_value;
    }
    return false;
}

static inline bool indy_set_error_ifneq(struct indy_result *res, const char *error, int64_t os_value, int64_t expected)
{
    bool ok = (os_value == expected);
    if (!ok)
        indy_set_error(res, error, os_value);
    return ok;
}

#endif
