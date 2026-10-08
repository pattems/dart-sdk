// Copyright (c) 2012, the Dart project authors.  Please see the AUTHORS file
// for details. All rights reserved. Use of this source code is governed by a
// BSD-style license that can be found in the LICENSE file.

#include "platform/globals.h"
#if defined(DART_HOST_OS_LINUX) || defined(DART_HOST_OS_ANDROID)

#include "bin/platform.h"

#include <errno.h>
#include <signal.h>
#include <string.h>
#include <sys/resource.h>
#if defined(DART_HOST_OS_ANDROID)
#include <sys/system_properties.h>
#endif
#include <sys/utsname.h>
#include <unistd.h>

#if defined(__FreeBSD__)
#include <pthread_np.h>
#include <sys/sysctl.h>
#else
#include <sys/prctl.h>
#endif

#include "bin/console.h"
#include "bin/dartutils.h"
#include "bin/file.h"

#if defined(__FreeBSD__)
extern char** environ;
#endif

namespace dart {
namespace bin {

const char* Platform::executable_name_ = nullptr;
int Platform::script_index_ = 1;
char** Platform::argv_ = nullptr;

static const char* strcode(int si_signo, int si_code) {
#define CASE(signo, code)                                                      \
  if (si_signo == signo && si_code == code) return #code;

  CASE(SIGILL, ILL_ILLOPC);
  CASE(SIGILL, ILL_ILLOPN);
  CASE(SIGILL, ILL_ILLADR);
  CASE(SIGILL, ILL_ILLTRP);
  CASE(SIGILL, ILL_PRVOPC);
  CASE(SIGILL, ILL_PRVREG);
  CASE(SIGILL, ILL_COPROC);
  CASE(SIGILL, ILL_BADSTK);
  CASE(SIGSEGV, SEGV_MAPERR);
  CASE(SIGSEGV, SEGV_ACCERR);
  CASE(SIGBUS, BUS_ADRALN);
  CASE(SIGBUS, BUS_ADRERR);
  CASE(SIGBUS, BUS_OBJERR);
#if defined(BUS_MCEERR_AR)  // Linux-specific machine check codes.
  CASE(SIGBUS, BUS_MCEERR_AR);
  CASE(SIGBUS, BUS_MCEERR_AO);
#endif
  CASE(SIGTRAP, TRAP_BRKPT);
  CASE(SIGTRAP, TRAP_TRACE);
#undef CASE
  return "?";
}

static void segv_handler(int signal, siginfo_t* siginfo, void* context) {
  Syslog::PrintErr(
      "\n===== CRASH =====\n"
      "si_signo=%s(%d), si_code=%s(%d), si_addr=%p\n",
      strsignal(siginfo->si_signo), siginfo->si_signo,
      strcode(siginfo->si_signo, siginfo->si_code), siginfo->si_code,
      siginfo->si_addr);
  Dart_DumpNativeStackTrace(context);
  Dart_PrepareToAbort();
  abort();
}

bool Platform::Initialize(bool install_crash_handler /* = true */) {
  // Turn off the signal handler for SIGPIPE as it causes the process
  // to terminate on writing to a closed pipe. Without the signal
  // handler error EPIPE is set instead.
  struct sigaction act = {};
  act.sa_handler = SIG_IGN;
  if (sigaction(SIGPIPE, &act, nullptr) != 0) {
    perror("Setting signal handler failed");
    return false;
  }

  // tcsetattr raises SIGTTOU if we try to set console attributes when
  // backgrounded, which suspends the process. Ignoring the signal prevents
  // us from being suspended and lets us fail gracefully instead.
  sigset_t signal_mask;
  sigemptyset(&signal_mask);
  sigaddset(&signal_mask, SIGTTOU);
  if (sigprocmask(SIG_BLOCK, &signal_mask, nullptr) < 0) {
    perror("Setting signal handler failed");
    return false;
  }

  if (install_crash_handler) {
    act = {};
    act.sa_flags = SA_SIGINFO;
    act.sa_sigaction = &segv_handler;

    if (sigemptyset(&act.sa_mask) != 0) {
      perror("sigemptyset() failed.");
      return false;
    }
    if (sigaddset(&act.sa_mask, SIGPROF) != 0) {
      perror("sigaddset() failed");
      return false;
    }
    if (sigaction(SIGSEGV, &act, nullptr) != 0) {
      perror("sigaction() failed.");
      return false;
    }
    if (sigaction(SIGBUS, &act, nullptr) != 0) {
      perror("sigaction() failed.");
      return false;
    }
    if (sigaction(SIGTRAP, &act, nullptr) != 0) {
      perror("sigaction() failed.");
      return false;
    }
    if (sigaction(SIGILL, &act, nullptr) != 0) {
      perror("sigaction() failed.");
      return false;
    }
  }
  return true;
}

int Platform::NumberOfProcessors() {
  return sysconf(_SC_NPROCESSORS_ONLN);
}

const char* Platform::OperatingSystemVersion() {
#if defined(DART_HOST_OS_ANDROID)
  char os_version[PROP_VALUE_MAX + 1];
  int os_version_length =
      __system_property_get("ro.build.display.id", os_version);
  if (os_version_length == 0) {
    return nullptr;
  }
  os_version[Utils::Minimum(os_version_length, PROP_VALUE_MAX)] = '\0';
  return DartUtils::ScopedCopyCString(os_version);
#else
  struct utsname info;
  int ret = uname(&info);
  if (ret != 0) {
    return nullptr;
  }
  return DartUtils::ScopedCStringFormatted("%s %s %s", info.sysname,
                                           info.release, info.version);
#endif
}

const char* Platform::LibraryPrefix() {
  return "lib";
}

const char* Platform::LibraryExtension() {
  return "so";
}

const char* Platform::LocaleName() {
  char* lang = getenv("LANG");
  if (lang == nullptr) {
    return "en_US";
  }
  return lang;
}

bool Platform::LocalHostname(char* buffer, intptr_t buffer_length) {
  return gethostname(buffer, buffer_length) == 0;
}

char** Platform::Environment(intptr_t* count) {
  // Using environ directly is only safe as long as we do not
  // provide access to modifying environment variables.
  intptr_t i = 0;
  char** tmp = environ;
  while (*(tmp++) != nullptr) {
    i++;
  }
  *count = i;
  char** result;
  result = reinterpret_cast<char**>(Dart_ScopeAllocate(i * sizeof(*result)));
  for (intptr_t current = 0; current < i; current++) {
    result[current] = environ[current];
  }
  return result;
}

const char* Platform::GetExecutableName() {
  return executable_name_;
}

#if defined(__FreeBSD__)
// FreeBSD does not mount procfs by default, so there is no /proc/self/exe.
// Ask the kernel for the executable's path instead. Like File::ReadLinkInto,
// returns the length including the terminating NUL, or -1 on failure.
static intptr_t GetExecutablePathInto(char* result, size_t result_size) {
  int mib[] = {CTL_KERN, KERN_PROC, KERN_PROC_PATHNAME, -1};
  size_t length = result_size;
  if (sysctl(mib, sizeof(mib) / sizeof(mib[0]), result, &length, nullptr, 0) !=
      0) {
    return -1;
  }
  return length;
}
#endif  // defined(__FreeBSD__)

const char* Platform::ResolveExecutablePath() {
#if defined(__FreeBSD__)
  char path[PATH_MAX + 1];
  const intptr_t length = GetExecutablePathInto(path, sizeof(path));
  if (length <= 0) {
    return nullptr;
  }
  char* result = DartUtils::ScopedCString(length);
  memmove(result, path, length);
  return result;
#else
  return File::ReadLink("/proc/self/exe");
#endif
}

intptr_t Platform::ResolveExecutablePathInto(char* result, size_t result_size) {
#if defined(__FreeBSD__)
  return GetExecutablePathInto(result, result_size);
#else
  return File::ReadLinkInto("/proc/self/exe", result, result_size);
#endif
}

void Platform::SetProcessName(const char* name) {
#if defined(__FreeBSD__)
  // Like PR_SET_NAME, this names the calling thread.
  pthread_setname_np(pthread_self(), name);
#else
  prctl(PR_SET_NAME, reinterpret_cast<unsigned long>(name), 0, 0, 0);  // NOLINT
#endif
}

void Platform::Exit(int exit_code) {
  Console::RestoreConfig();
  Dart_PrepareToAbort();
  exit(exit_code);
}

void Platform::_Exit(int exit_code) {
  Console::RestoreConfig();
  Dart_PrepareToAbort();
  _exit(exit_code);
}

void Platform::SetCoreDumpResourceLimit(int value) {
  rlimit limit = {static_cast<rlim_t>(value), static_cast<rlim_t>(value)};
  setrlimit(RLIMIT_CORE, &limit);
}

bool Platform::SetEnvironmentVariable(const char* name, const char* value) {
  if (value == nullptr) {
    return unsetenv(name) == 0;
  }
  return setenv(name, value, 0) == 0;
}

}  // namespace bin
}  // namespace dart

#endif  // defined(DART_HOST_OS_LINUX) || defined(DART_HOST_OS_ANDROID)
