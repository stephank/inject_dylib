// inject_dylib, https://github.com/stephank/inject_dylib
// Copyright (c) 2015 Stéphan Kochen
// See the README.md for license details.

#include "indy_private.h"

#include <mach/mach.h>
#include <mach-o/loader.h>
#include <mach-o/nlist.h>

// dyld rejects dylibs smaller than this.
#define MIN_MACH_O_SIZE 4096

// Target arch dyld_image_info structs.
struct indy_dyld_image_info_x {
    /* const struct mach_header* */ indy_uintptr_x imageLoadAddress;
    /* const char* */               indy_uintptr_x imageFilePath;
    /* uintptr_t */                 indy_uintptr_x imageFileModDate;
};
struct indy_dyld_all_image_infos_x {
    uint32_t version;
    uint32_t infoArrayCount;
    /* const struct dyld_image_info* */ indy_uintptr_x infoArray;
};

static void indy_symbols_in_image_x(mach_port_name_t task_port, struct indy_link *match,
                                    mach_vm_address_t addr, struct indy_mach_header_x *mh);

bool indy_symbols_x(struct indy_private *p, struct indy_link *match)
{
    kern_return_t kret;

    mach_vm_address_t region_addr = 0;
    mach_vm_size_t region_size = 0;

    struct indy_mach_header_x mh;
    mach_vm_size_t mh_size;

    // Look for dyld. We assume dyld is always mapped in its own region, and
    // the Mach-O header is at the start of that region.
    for (;;)
    {
        // Get the next region.
        natural_t region_depth = 0;
        struct indy_vm_region_submap_info_x region_info;
        mach_msg_type_number_t region_info_count = INDY_VM_REGION_SUBMAP_INFO_COUNT_X;
        kret = mach_vm_region_recurse(p->task, &region_addr, &region_size, &region_depth,
                                      (vm_region_recurse_info_t) &region_info, &region_info_count);
        if (kret == KERN_INVALID_ADDRESS)
            return indy_set_error(p->res, indy_couldnt_locate_dynamic_loader, 0);
        else if (kret != KERN_SUCCESS)
            return indy_set_error(p->res, indy_couldnt_iterate_target_memory, kret);

        // Filter on a minimum Mach-O size, and read/exe flags.
        if (region_size >= MIN_MACH_O_SIZE &&
            (region_info.protection & VM_PROT_READ) &&
            (region_info.protection & VM_PROT_EXECUTE))
        {
            // Read the Mach-O header.
            kret = mach_vm_read_overwrite(p->task, region_addr, sizeof(mh),
                                          (mach_vm_address_t) &mh, &mh_size);

            // Check for a Mach-O with the right filetype.
            if (kret == KERN_SUCCESS && mh_size == sizeof(mh) &&
                    mh.magic == INDY_MH_MAGIC_X && mh.filetype == MH_DYLINKER)
                break;
        }

        // Move past this region.
        region_addr += region_size;
    }

    // Quick bounds check of commands.
    if (sizeof(mh) + mh.sizeofcmds > region_size)
        return indy_set_error(p->res, indy_couldnt_locate_dynamic_loader, 0);

    // dyld exports a debugger interface documented in <mach-o/dyld_images.h>.
    // Normally, a debugger would call _dyld_all_image_infos, but we don't want
    // to run code. We assume it's always a simple getter for dyld::gProcessInfo,
    // and instead reuse find_symbols_in_image to locate it directly.
    indy_uintptr_x all_infos_ptr_addr = 0;
    struct indy_link_symbol dyld_symbols[] = {
        { .out = &all_infos_ptr_addr, .name = "__ZN4dyld12gProcessInfoE" }
    };
    struct indy_link_image dyld_images[] = {
        { .name = "/usr/lib/dyld", .num_symbols = 1, .symbols = dyld_symbols }
    };
    struct indy_link dyld_match = {
        .num_images = 1,
        .images = dyld_images
    };
    indy_symbols_in_image_x(p->task, &dyld_match, region_addr, &mh);
    if (all_infos_ptr_addr == 0)
        return indy_set_error(p->res, indy_couldnt_locate_image_info, 0);

    // Read the dyld::gProcessInfo pointer.
    indy_uintptr_x all_infos_ptr;
    mach_vm_size_t all_infos_ptr_size;
    kret = mach_vm_read_overwrite(p->task, all_infos_ptr_addr, sizeof(all_infos_ptr),
                                  (mach_vm_address_t) &all_infos_ptr, &all_infos_ptr_size);
    if (kret != KERN_SUCCESS || all_infos_ptr_size != sizeof(all_infos_ptr))
        return indy_set_error(p->res, indy_couldnt_read_image_info, kret);

    // Read the image infos struct.
    struct indy_dyld_all_image_infos_x all_infos;
    mach_vm_size_t all_infos_size;
    kret = mach_vm_read_overwrite(p->task, all_infos_ptr, sizeof(all_infos),
                                  (mach_vm_address_t) &all_infos, &all_infos_size);
    if (kret != KERN_SUCCESS || all_infos_size != sizeof(all_infos))
        return indy_set_error(p->res, indy_couldnt_read_image_info, kret);

    // Read the image infos array.
    uint32_t infos_count = all_infos.infoArrayCount;
    struct indy_dyld_image_info_x infos[infos_count];
    mach_vm_size_t infos_size;
    kret = mach_vm_read_overwrite(p->task, all_infos.infoArray, sizeof(infos),
                                  (mach_vm_address_t) &infos, &infos_size);
    if (kret != KERN_SUCCESS || infos_size != sizeof(infos))
        return indy_set_error(p->res, indy_couldnt_read_image_info, kret);

    // Walk all images.
    for (uint32_t i = 0; i < infos_count; i++)
    {
        mach_vm_address_t base = infos[i].imageLoadAddress;

        // Read the Mach-O header.
        kret = mach_vm_read_overwrite(p->task, base, sizeof(mh),
                                      (mach_vm_address_t) &mh, &mh_size);

        // Check for a Mach-O with the right filetype.
        if (kret == KERN_SUCCESS && mh_size == sizeof(mh) &&
                mh.magic == INDY_MH_MAGIC_X && mh.filetype == MH_DYLIB)
            // Match symbols in this image.
            indy_symbols_in_image_x(p->task, match, base, &mh);
    }

    // Check that we found everything.
    for (size_t i = 0; i < match->num_images; i++)
        for (size_t j = 0; j < match->images[i].num_symbols; j++)
            if (*(indy_uintptr_x *) match->images[i].symbols[j].out == 0)
                return indy_set_error(p->res, indy_couldnt_locate_symbols, 0);

    return true;
}

static void indy_symbols_in_image_x(mach_port_name_t task_port, struct indy_link *match,
                                    mach_vm_address_t addr, struct indy_mach_header_x *mh)
{
    kern_return_t kret;

    // Read the load commands.
    void *lc_data = malloc(mh->sizeofcmds);
    mach_vm_size_t lc_size;
    kret = mach_vm_read_overwrite(task_port, addr + sizeof(struct indy_mach_header_x), mh->sizeofcmds,
                                  (mach_vm_address_t) lc_data, &lc_size);
    if (kret != KERN_SUCCESS || lc_size != mh->sizeofcmds) {
        free(lc_data);
        return;
    }

    // Find the install name
    const char *name = NULL;
    size_t name_len = 0;
    if (mh->filetype == MH_DYLINKER)
    {
        // Special case for dyld.
        name = "/usr/lib/dyld";
        name_len = 13;
    }
    else
    {
        uint8_t *lc_ptr = lc_data;
        uint8_t *lc_end = lc_ptr + lc_size;
        while (lc_ptr < lc_end)
        {
            struct load_command *lc = (void *) lc_ptr;
            if (lc->cmd == LC_ID_DYLIB)
            {
                struct dylib_command *lc_dylib = (void *) lc;
                uint32_t name_off = lc_dylib->dylib.name.offset;
                name = (void *) (((uint8_t *) lc_dylib) + name_off);
                name_len = lc_dylib->cmdsize - name_off;
                break;
            }
            lc_ptr += lc->cmdsize;
        }
    }

    // Match with an image.
    struct indy_link_image *image = NULL;
    if (name)
    {
        for (uint32_t i = 0; i < match->num_images; i++)
        {
            if (strncmp(match->images[i].name, name, name_len) == 0)
            {
                image = &match->images[i];
                break;
            }
        }
    }

    // Find the LC_SYMTAB, the segment with fileoff=0, and the LINKEDIT segment.
    struct symtab_command *lc_symtab = NULL;
    struct indy_segment_command_x *lc_segment_zero = NULL;
    struct indy_segment_command_x *lc_segment_linkedit = NULL;
    if (image)
    {
        uint8_t *lc_ptr = lc_data;
        uint8_t *lc_end = lc_ptr + lc_size;
        while (lc_ptr < lc_end)
        {
            struct dylib_command *lc = (void *) lc_ptr;
            switch (lc->cmd)
            {
                case LC_SYMTAB:
                    lc_symtab = (void *) lc;
                    break;

                case INDY_LC_SEGMENT_X: {
                    struct indy_segment_command_x *lc_segment = (void *) lc;
                    if (lc_segment->fileoff == 0)
                        lc_segment_zero = lc_segment;
                    if (strncmp(lc_segment->segname, "__LINKEDIT", sizeof(lc_segment->segname)) == 0)
                        lc_segment_linkedit = lc_segment;
                    break;
                }
            }
            lc_ptr += lc->cmdsize;
        }
    }
    bool ok = lc_symtab && lc_segment_zero && lc_segment_linkedit;

    mach_vm_address_t slide = 0;
    mach_vm_address_t laddr_linkedit = 0;
    if (ok)
    {
        // The zero segment is actually at addr. The diff between addr and vmaddr
        // is the amount the image has slid. This may underflow, but that's okay.
        slide = addr - lc_segment_zero->vmaddr;

        // Map LINKEDIT anywhere into our address space.
        vm_prot_t cur_protection, max_protection;
        kret = mach_vm_remap(mach_task_self(), &laddr_linkedit, lc_segment_linkedit->vmsize, 4, 1,
                             task_port, lc_segment_linkedit->vmaddr + slide, 0,
                             &cur_protection, &max_protection, VM_INHERIT_NONE);
        ok = (kret == KERN_SUCCESS);
    }

    mach_vm_address_t lbase_linkedit = 0;
    struct indy_nlist_x *symbols = NULL;
    const char *strings = NULL;
    if (ok) {
        // Translate file offsets to addresses.
        lbase_linkedit = laddr_linkedit - lc_segment_linkedit->fileoff;
        symbols = (void *) (lbase_linkedit + lc_symtab->symoff);
        strings = (void *) (lbase_linkedit + lc_symtab->stroff);

        // Quick bounds check.
        mach_vm_address_t lend_linkedit = laddr_linkedit + lc_segment_linkedit->vmsize;
        ok = ((mach_vm_address_t) (symbols + lc_symtab->nsyms)   < lend_linkedit) &&
             ((mach_vm_address_t) (strings + lc_symtab->strsize) < lend_linkedit);
    }

    if (ok) {
        // Match symbol names.
        for (uint32_t i = 0; i < lc_symtab->nsyms; i++)
        {
            if (symbols[i].n_value == 0 || symbols[i].n_un.n_strx == 0 ||
                    symbols[i].n_un.n_strx >= lc_symtab->strsize)
                continue;

            const char *name = strings + symbols[i].n_un.n_strx;
            for (size_t j = 0; j < image->num_symbols; j++)
            {
                if (strcmp(image->symbols[j].name, name) == 0)
                {
                    *(indy_uintptr_x *) image->symbols[j].out =
                        (indy_uintptr_x) (symbols[i].n_value + slide);
                    break;
                }
            }
        }
    }

    // Cleanup.
    if (laddr_linkedit != 0)
        vm_deallocate(mach_task_self(), laddr_linkedit, lc_segment_linkedit->vmsize);
    free(lc_data);
}
