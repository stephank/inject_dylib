// inject_dylib, https://github.com/stephank/inject_dylib
// Copyright (c) 2015 Stéphan Kochen
// See the README.md for license details.

#define INDY_VM_REGION_SUBMAP_INFO_COUNT_X VM_REGION_SUBMAP_INFO_COUNT
#define INDY_MH_MAGIC_X MH_MAGIC
#define INDY_LC_SEGMENT_X LC_SEGMENT
#define indy_uintptr_x indy_uintptr_32
#define indy_vm_region_submap_info_x vm_region_submap_info
#define indy_mach_header_x mach_header
#define indy_segment_command_x segment_command
#define indy_nlist_x nlist
#define indy_symbols_x indy_symbols_32
#define indy_symbols_in_image_x indy_symbols_in_image_32

#include "indy_symbols.inc.c"
