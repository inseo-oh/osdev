#include "unistd.h"
#include <isos/syscall.h>

// https://pubs.opengroup.org/onlinepubs/9699919799/functions/read.html
ssize_t read(int fildes, void *buf, size_t nbyte) {
        return syscall3(SYSCALL_INDEX_READ, fildes, (uintptr_t)buf, nbyte);
}
