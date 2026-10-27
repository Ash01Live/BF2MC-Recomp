// Kernel exports BF2:MC imports that the SDK does not implement yet. Each registers itself at startup the same way
// the SDK's own exports do (KERNEL_EXPORT), so the game calls them like any other kernel function.

#include <cstring>

#include "kernel/kernel.h"
#include "kernel/exports.h"

namespace kernel {

// NTSTATUS IoQueryVolumeInformation(PFILE_OBJECT FileObject, FS_INFORMATION_CLASS FsInformationClass, ULONG Length,
//                                   PVOID FsInformation, PULONG ReturnedLength)
//
// The file-object form of NtQueryVolumeInformationFile; XAPI reaches it while it probes the hard disk partitions at
// boot (return address 0x00016107). Answers the same classes with the same made-up volume as the SDK's
// NtQueryVolumeInformationFile: 16 GiB, 8 GiB free, FATX.
KERNEL_EXPORT(76, IoQueryVolumeInformation, 5) {
    uint32_t cls = a[1], len = a[2], out = a[3], returned = a[4];
    auto answer = [&](uint32_t need) -> uint32_t {
        if (returned) W32(returned, need);
        return kSuccess;
    };
    auto fits = [&](uint32_t need) {
        if (len < need) return false;
        std::memset(HostPtr(out), 0, need);
        return true;
    };
    switch (cls) {
        case 3:  // FileFsSizeInformation: total and free allocation units, sectors per unit, bytes per sector
            if (!fits(0x18)) return kBufferTooSmall;
            W64(out, 0x100000);
            W64(out + 8, 0x80000);
            W32(out + 0x10, 32);
            W32(out + 0x14, 512);
            return answer(0x18);
        case 7:  // FileFsFullSizeInformation
            if (!fits(0x20)) return kBufferTooSmall;
            W64(out, 0x100000);
            W64(out + 8, 0x80000);
            W64(out + 0x10, 0x80000);
            W32(out + 0x18, 32);
            W32(out + 0x1C, 512);
            return answer(0x20);
        case 1:  // FileFsVolumeInformation: serial number, no label
            if (!fits(0x18)) return kBufferTooSmall;
            W32(out + 8, 0x12345678);
            return answer(0x18);
        case 5:  // FileFsAttributeInformation: attributes, max name length, name length, "FATX"
            if (!fits(16)) return kBufferTooSmall;
            W32(out, 0);
            W32(out + 4, 42);
            W32(out + 8, 4);
            std::memcpy(HostPtr(out + 12), "FATX", 4);
            return answer(16);
        default:
            LOG_FIRST(8, "IoQueryVolumeInformation: class %u", cls);
            return kInvalidParameter;
    }
}

}  // namespace kernel
