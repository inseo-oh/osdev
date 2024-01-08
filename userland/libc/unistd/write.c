#include "unistd.h"
#include <isos/syscall.h>

// https://pubs.opengroup.org/onlinepubs/9699919799/functions/write.html
ssize_t write(int fildes, void const *buf, size_t nbyte) {
        return syscall3(SYSCALL_INDEX_WRITE, fildes, (uintptr_t)buf, nbyte);
}
