// Copyright (c) 2026, the Dart project authors.  Please see the AUTHORS file
// for details. All rights reserved. Use of this source code is governed by a
// BSD-style license that can be found in the LICENSE file.

#include "vm/globals.h"
#include "vm/instructions.h"
#include "vm/signal_handler.h"
#include "vm/simulator.h"
#if defined(DART_HOST_OS_BSD)

namespace dart {

uintptr_t SignalHandler::GetProgramCounter(const mcontext_t& mcontext) {
  uintptr_t pc = 0;

#if defined(HOST_ARCH_IA32)
  pc = static_cast<uintptr_t>(mcontext.mc_eip);
#elif defined(HOST_ARCH_X64)
  pc = static_cast<uintptr_t>(mcontext.mc_rip);
#elif defined(HOST_ARCH_ARM)
  pc = static_cast<uintptr_t>(mcontext.__gregs[_REG_PC]);
#elif defined(HOST_ARCH_ARM64)
  pc = static_cast<uintptr_t>(mcontext.mc_gpregs.gp_elr);
#elif defined(HOST_ARCH_RISCV64)
  pc = static_cast<uintptr_t>(mcontext.mc_gpregs.gp_sepc);
#else
#error Unsupported architecture.
#endif  // HOST_ARCH_...
  return pc;
}

uintptr_t SignalHandler::GetFramePointer(const mcontext_t& mcontext) {
  uintptr_t fp = 0;

#if defined(HOST_ARCH_IA32)
  fp = static_cast<uintptr_t>(mcontext.mc_ebp);
#elif defined(HOST_ARCH_X64)
  fp = static_cast<uintptr_t>(mcontext.mc_rbp);
#elif defined(HOST_ARCH_ARM)
  // B1.3.3 Program Status Registers (PSRs)
  if ((mcontext.__gregs[_REG_CPSR] & (1 << 5)) != 0) {
    // Thumb mode.
    fp = static_cast<uintptr_t>(mcontext.__gregs[_REG_R7]);
  } else {
    // ARM mode.
    fp = static_cast<uintptr_t>(mcontext.__gregs[_REG_FP]);
  }
#elif defined(HOST_ARCH_ARM64)
  fp = static_cast<uintptr_t>(mcontext.mc_gpregs.gp_x[29]);
#elif defined(HOST_ARCH_RISCV64)
  fp = static_cast<uintptr_t>(mcontext.mc_gpregs.gp_s[0]);
#else
#error Unsupported architecture.
#endif  // HOST_ARCH_...

  return fp;
}

uintptr_t SignalHandler::GetCStackPointer(const mcontext_t& mcontext) {
  uintptr_t sp = 0;

#if defined(HOST_ARCH_IA32)
  sp = static_cast<uintptr_t>(mcontext.mc_esp);
#elif defined(HOST_ARCH_X64)
  sp = static_cast<uintptr_t>(mcontext.mc_rsp);
#elif defined(HOST_ARCH_ARM)
  sp = static_cast<uintptr_t>(mcontext.__gregs[_REG_SP]);
#elif defined(HOST_ARCH_ARM64)
  sp = static_cast<uintptr_t>(mcontext.mc_gpregs.gp_sp);
#elif defined(HOST_ARCH_RISCV64)
  sp = static_cast<uintptr_t>(mcontext.mc_gpregs.gp_sp);
#else
#error Unsupported architecture.
#endif  // HOST_ARCH_...
  return sp;
}

uintptr_t SignalHandler::GetDartStackPointer(const mcontext_t& mcontext) {
#if defined(TARGET_ARCH_ARM64) && !defined(DART_INCLUDE_SIMULATOR)
  return static_cast<uintptr_t>(mcontext.mc_gpregs.gp_x[SPREG]);
#else
  return GetCStackPointer(mcontext);
#endif
}

uintptr_t SignalHandler::GetLinkRegister(const mcontext_t& mcontext) {
  uintptr_t lr = 0;

#if defined(HOST_ARCH_IA32)
  lr = 0;
#elif defined(HOST_ARCH_X64)
  lr = 0;
#elif defined(HOST_ARCH_ARM)
  lr = static_cast<uintptr_t>(mcontext.__gregs[_REG_LR]);
#elif defined(HOST_ARCH_ARM64)
  lr = static_cast<uintptr_t>(mcontext.mc_gpregs.gp_lr);
#elif defined(HOST_ARCH_RISCV64)
  lr = static_cast<uintptr_t>(mcontext.mc_gpregs.gp_ra);
#else
#error Unsupported architecture.
#endif  // HOST_ARCH_...
  return lr;
}

void SignalHandler::Install(SignalAction action) {
  struct sigaction act = {};
  act.sa_handler = nullptr;
  act.sa_sigaction = action;
  sigemptyset(&act.sa_mask);
  sigaddset(&act.sa_mask, SIGPROF);  // Prevent nested signals.
  act.sa_flags = SA_RESTART | SA_SIGINFO;
  int r = sigaction(SIGPROF, &act, nullptr);
  ASSERT(r == 0);
}

void SignalHandler::Remove() {
  // Ignore future SIGPROF signals because by default SIGPROF will terminate
  // the process and we may have some signals in flight.
  struct sigaction act = {};
  act.sa_handler = SIG_IGN;
  sigemptyset(&act.sa_mask);
  act.sa_flags = 0;
  int r = sigaction(SIGPROF, &act, nullptr);
  ASSERT(r == 0);
}

}  // namespace dart

#endif  // defined(DART_HOST_OS_BSD)
