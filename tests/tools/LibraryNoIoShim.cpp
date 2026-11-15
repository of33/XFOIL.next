// XFOIL.next
//
// Copyright (C) 2026 Olaf Frommann
// https://github.com/of33/XFOIL.next
//
// XFOIL.next is based on XFOIL 6.99 by Mark Drela and
// Harold Youngren.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//
// SPDX-License-Identifier: GPL-3.0-or-later

// File: tests/tools/LibraryNoIoShim.cpp
// Test-only libc interposition for the L7 armed no-I/O proof. The shim
// is disarmed for loader startup and harness operations. Only the
// explicit arm window is observed.

#include "LibraryNoIoShim.h"

#include <atomic>
#include <cerrno>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

namespace {

std::atomic<bool> armed{false};
std::atomic<unsigned long long> hitCount{0};
std::atomic<const char*> firstHit{nullptr};

void record(const char* name) {
  hitCount.fetch_add(1, std::memory_order_relaxed);
  const char* expected = nullptr;
  firstHit.compare_exchange_strong(expected, name,
                                   std::memory_order_relaxed);
}

template <class Function>
Function nextSymbol(const char* name) {
  return reinterpret_cast<Function>(dlsym(RTLD_NEXT, name));
}

bool active() {
  return armed.load(std::memory_order_acquire);
}

int denyInt(const char* name) {
  record(name);
  errno = EACCES;
  return -1;
}

}  // namespace

extern "C" void xfoil_noio_reset(void) {
  armed.store(false, std::memory_order_release);
  hitCount.store(0, std::memory_order_relaxed);
  firstHit.store(nullptr, std::memory_order_relaxed);
}

extern "C" void xfoil_noio_arm(void) {
  hitCount.store(0, std::memory_order_relaxed);
  firstHit.store(nullptr, std::memory_order_relaxed);
  armed.store(true, std::memory_order_release);
}

extern "C" void xfoil_noio_disarm(void) {
  armed.store(false, std::memory_order_release);
}

extern "C" unsigned long long xfoil_noio_hits(void) {
  return hitCount.load(std::memory_order_relaxed);
}

extern "C" const char* xfoil_noio_first_hit(void) {
  const char* hit = firstHit.load(std::memory_order_relaxed);
  return hit == nullptr ? "" : hit;
}

extern "C" int open(const char* path, int flags, ...) {
  if (active()) {
    return denyInt("open");
  }
  using Function = int (*)(const char*, int, ...);
  static Function real = nextSymbol<Function>("open");
  if ((flags & O_CREAT) != 0) {
    va_list args;
    va_start(args, flags);
    const mode_t mode = static_cast<mode_t>(va_arg(args, unsigned int));
    va_end(args);
    return real(path, flags, mode);
  }
  return real(path, flags);
}

extern "C" int open64(const char* path, int flags, ...) {
  if (active()) {
    return denyInt("open64");
  }
  using Function = int (*)(const char*, int, ...);
  static Function real = nextSymbol<Function>("open64");
  if ((flags & O_CREAT) != 0) {
    va_list args;
    va_start(args, flags);
    const mode_t mode = static_cast<mode_t>(va_arg(args, unsigned int));
    va_end(args);
    return real(path, flags, mode);
  }
  return real(path, flags);
}

extern "C" int openat(int dirfd, const char* path, int flags, ...) {
  if (active()) {
    return denyInt("openat");
  }
  using Function = int (*)(int, const char*, int, ...);
  static Function real = nextSymbol<Function>("openat");
  if ((flags & O_CREAT) != 0) {
    va_list args;
    va_start(args, flags);
    const mode_t mode = static_cast<mode_t>(va_arg(args, unsigned int));
    va_end(args);
    return real(dirfd, path, flags, mode);
  }
  return real(dirfd, path, flags);
}

extern "C" int creat(const char* path, mode_t mode) {
  if (active()) {
    return denyInt("creat");
  }
  using Function = int (*)(const char*, mode_t);
  static Function real = nextSymbol<Function>("creat");
  return real(path, mode);
}

extern "C" FILE* fopen(const char* path, const char* mode) {
  if (active()) {
    record("fopen");
    errno = EACCES;
    return nullptr;
  }
  using Function = FILE* (*)(const char*, const char*);
  static Function real = nextSymbol<Function>("fopen");
  return real(path, mode);
}

extern "C" FILE* freopen(const char* path, const char* mode, FILE* stream) {
  if (active()) {
    record("freopen");
    errno = EACCES;
    return nullptr;
  }
  using Function = FILE* (*)(const char*, const char*, FILE*);
  static Function real = nextSymbol<Function>("freopen");
  return real(path, mode, stream);
}

extern "C" int rename(const char* oldPath, const char* newPath) {
  if (active()) {
    return denyInt("rename");
  }
  using Function = int (*)(const char*, const char*);
  static Function real = nextSymbol<Function>("rename");
  return real(oldPath, newPath);
}

extern "C" int unlink(const char* path) {
  if (active()) {
    return denyInt("unlink");
  }
  using Function = int (*)(const char*);
  static Function real = nextSymbol<Function>("unlink");
  return real(path);
}

extern "C" int remove(const char* path) {
  if (active()) {
    return denyInt("remove");
  }
  using Function = int (*)(const char*);
  static Function real = nextSymbol<Function>("remove");
  return real(path);
}

extern "C" int stat(const char* path, struct stat* info) {
  if (active()) {
    return denyInt("stat");
  }
  using Function = int (*)(const char*, struct stat*);
  static Function real = nextSymbol<Function>("stat");
  return real(path, info);
}

extern "C" int lstat(const char* path, struct stat* info) {
  if (active()) {
    return denyInt("lstat");
  }
  using Function = int (*)(const char*, struct stat*);
  static Function real = nextSymbol<Function>("lstat");
  return real(path, info);
}

extern "C" int access(const char* path, int mode) {
  if (active()) {
    return denyInt("access");
  }
  using Function = int (*)(const char*, int);
  static Function real = nextSymbol<Function>("access");
  return real(path, mode);
}

extern "C" int mkdir(const char* path, mode_t mode) {
  if (active()) {
    return denyInt("mkdir");
  }
  using Function = int (*)(const char*, mode_t);
  static Function real = nextSymbol<Function>("mkdir");
  return real(path, mode);
}

extern "C" int rmdir(const char* path) {
  if (active()) {
    return denyInt("rmdir");
  }
  using Function = int (*)(const char*);
  static Function real = nextSymbol<Function>("rmdir");
  return real(path);
}

extern "C" int chdir(const char* path) {
  if (active()) {
    return denyInt("chdir");
  }
  using Function = int (*)(const char*);
  static Function real = nextSymbol<Function>("chdir");
  return real(path);
}

extern "C" char* getcwd(char* buffer, size_t size) {
  if (active()) {
    record("getcwd");
    errno = EACCES;
    return nullptr;
  }
  using Function = char* (*)(char*, size_t);
  static Function real = nextSymbol<Function>("getcwd");
  return real(buffer, size);
}

extern "C" char* getenv(const char* name) {
  if (active()) {
    record("getenv");
    return nullptr;
  }
  using Function = char* (*)(const char*);
  static Function real = nextSymbol<Function>("getenv");
  return real(name);
}

extern "C" int setenv(const char* name, const char* value, int overwrite) {
  if (active()) {
    return denyInt("setenv");
  }
  using Function = int (*)(const char*, const char*, int);
  static Function real = nextSymbol<Function>("setenv");
  return real(name, value, overwrite);
}

extern "C" int putenv(char* value) {
  if (active()) {
    return denyInt("putenv");
  }
  using Function = int (*)(char*);
  static Function real = nextSymbol<Function>("putenv");
  return real(value);
}

extern "C" int unsetenv(const char* name) {
  if (active()) {
    return denyInt("unsetenv");
  }
  using Function = int (*)(const char*);
  static Function real = nextSymbol<Function>("unsetenv");
  return real(name);
}

extern "C" FILE* tmpfile(void) {
  if (active()) {
    record("tmpfile");
    errno = EACCES;
    return nullptr;
  }
  using Function = FILE* (*)(void);
  static Function real = nextSymbol<Function>("tmpfile");
  return real();
}

extern "C" int mkstemp(char* templateName) {
  if (active()) {
    return denyInt("mkstemp");
  }
  using Function = int (*)(char*);
  static Function real = nextSymbol<Function>("mkstemp");
  return real(templateName);
}

extern "C" int system(const char* command) {
  if (active()) {
    return denyInt("system");
  }
  using Function = int (*)(const char*);
  static Function real = nextSymbol<Function>("system");
  return real(command);
}

extern "C" FILE* popen(const char* command, const char* mode) {
  if (active()) {
    record("popen");
    errno = EACCES;
    return nullptr;
  }
  using Function = FILE* (*)(const char*, const char*);
  static Function real = nextSymbol<Function>("popen");
  return real(command, mode);
}

extern "C" pid_t fork(void) {
  if (active()) {
    return static_cast<pid_t>(denyInt("fork"));
  }
  using Function = pid_t (*)(void);
  static Function real = nextSymbol<Function>("fork");
  return real();
}

extern "C" int execve(const char* path, char* const argv[],
                      char* const envp[]) {
  if (active()) {
    return denyInt("execve");
  }
  using Function = int (*)(const char*, char* const[], char* const[]);
  static Function real = nextSymbol<Function>("execve");
  return real(path, argv, envp);
}

extern "C" int execv(const char* path, char* const argv[]) {
  if (active()) {
    return denyInt("execv");
  }
  using Function = int (*)(const char*, char* const[]);
  static Function real = nextSymbol<Function>("execv");
  return real(path, argv);
}

extern "C" int execl(const char* path, const char* arg, ...) {
  (void)path;
  (void)arg;
  if (active()) {
    return denyInt("execl");
  }
  return -1;
}

extern "C" int execlp(const char* file, const char* arg, ...) {
  (void)file;
  (void)arg;
  if (active()) {
    return denyInt("execlp");
  }
  return -1;
}

extern "C" int execvp(const char* file, char* const argv[]) {
  if (active()) {
    return denyInt("execvp");
  }
  using Function = int (*)(const char*, char* const[]);
  static Function real = nextSymbol<Function>("execvp");
  return real(file, argv);
}

extern "C" ssize_t write(int fd, const void* buffer, size_t count) {
  if (active()) {
    if (fd == STDOUT_FILENO) {
      record("write-stdout");
    } else if (fd == STDERR_FILENO) {
      record("write-stderr");
    } else {
      record("write");
    }
    errno = EACCES;
    return -1;
  }
  using Function = ssize_t (*)(int, const void*, size_t);
  static Function real = nextSymbol<Function>("write");
  return real(fd, buffer, count);
}

extern "C" size_t fwrite(const void* buffer, size_t size, size_t count,
                         FILE* stream) {
  if (active()) {
    record("fwrite");
    return 0;
  }
  using Function = size_t (*)(const void*, size_t, size_t, FILE*);
  static Function real = nextSymbol<Function>("fwrite");
  return real(buffer, size, count, stream);
}

extern "C" int fputs(const char* text, FILE* stream) {
  if (active()) {
    return denyInt("fputs");
  }
  using Function = int (*)(const char*, FILE*);
  static Function real = nextSymbol<Function>("fputs");
  return real(text, stream);
}

extern "C" int puts(const char* text) {
  if (active()) {
    return denyInt("puts");
  }
  using Function = int (*)(const char*);
  static Function real = nextSymbol<Function>("puts");
  return real(text);
}

extern "C" int fputc(int character, FILE* stream) {
  if (active()) {
    return denyInt("fputc");
  }
  using Function = int (*)(int, FILE*);
  static Function real = nextSymbol<Function>("fputc");
  return real(character, stream);
}

extern "C" int putchar(int character) {
  if (active()) {
    return denyInt("putchar");
  }
  using Function = int (*)(int);
  static Function real = nextSymbol<Function>("putchar");
  return real(character);
}

extern "C" int fflush(FILE* stream) {
  if (active()) {
    return denyInt("fflush");
  }
  using Function = int (*)(FILE*);
  static Function real = nextSymbol<Function>("fflush");
  return real(stream);
}

extern "C" int printf(const char* format, ...) {
  (void)format;
  if (active()) {
    return denyInt("printf");
  }
  return -1;
}

extern "C" int fprintf(FILE* stream, const char* format, ...) {
  (void)stream;
  (void)format;
  if (active()) {
    return denyInt("fprintf");
  }
  return -1;
}

extern "C" void perror(const char* text) {
  if (active()) {
    record("perror");
    return;
  }
  using Function = void (*)(const char*);
  static Function real = nextSymbol<Function>("perror");
  real(text);
}

extern "C" void abort(void) {
  if (active()) {
    record("abort");
  }
  using Function = void (*)(void);
  static Function real = nextSymbol<Function>("abort");
  real();
  __builtin_unreachable();
}

extern "C" void exit(int status) {
  if (active()) {
    record("exit");
  }
  using Function = void (*)(int);
  static Function real = nextSymbol<Function>("exit");
  real(status);
  __builtin_unreachable();
}

extern "C" void _exit(int status) {
  if (active()) {
    record("_exit");
  }
  using Function = void (*)(int);
  static Function real = nextSymbol<Function>("_exit");
  real(status);
  __builtin_unreachable();
}

extern "C" void quick_exit(int status) {
  if (active()) {
    record("quick_exit");
  }
  using Function = void (*)(int);
  static Function real = nextSymbol<Function>("quick_exit");
  real(status);
  __builtin_unreachable();
}

// End: tests/tools/LibraryNoIoShim.cpp
