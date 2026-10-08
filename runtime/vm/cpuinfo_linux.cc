// Copyright (c) 2012, the Dart project authors.  Please see the AUTHORS file
// for details. All rights reserved. Use of this source code is governed by a
// BSD-style license that can be found in the LICENSE file.

#include "vm/globals.h"
#if defined(DART_HOST_OS_LINUX)

#if defined(__FreeBSD__)
#include <sys/sysctl.h>  // NOLINT
#include <sys/types.h>   // NOLINT
#endif

#include "vm/cpuid.h"
#include "vm/cpuinfo.h"
#include "vm/proccpuinfo.h"

#include "platform/assert.h"

// As with Windows, on IA32 and X64, we use the cpuid instruction.
// The analogous instruction is privileged on ARM, so we resort to
// reading from /proc/cpuinfo.

namespace dart {

#if defined(__FreeBSD__)
// FreeBSD has no /proc/cpuinfo, so kCpuInfoSystem reads sysctls instead, as
// on macOS. Returns nullptr if the sysctl does not exist. Caller is
// responsible for freeing the result.
static char* ReadSysctlString(const char* name) {
  size_t length = 0;
  if (name == nullptr ||
      sysctlbyname(name, nullptr, &length, nullptr, 0) != 0) {
    return nullptr;
  }
  char* result = reinterpret_cast<char*>(malloc(length));
  if (sysctlbyname(name, result, &length, nullptr, 0) != 0) {
    free(result);
    return nullptr;
  }
  return result;
}
#endif

CpuInfoMethod CpuInfo::method_ = kCpuInfoDefault;
const char* CpuInfo::fields_[kCpuInfoMax] = {};

void CpuInfo::Init() {
#if defined(HOST_ARCH_IA32) || defined(HOST_ARCH_X64)
  fields_[kCpuInfoProcessor] = "vendor_id";
  fields_[kCpuInfoModel] = "model name";
  fields_[kCpuInfoHardware] = "model name";
  fields_[kCpuInfoFeatures] = "flags";
  fields_[kCpuInfoArchitecture] = "CPU architecture";
  method_ = kCpuInfoCpuId;
  CpuId::Init();
#elif defined(HOST_ARCH_ARM)
  fields_[kCpuInfoProcessor] = "Processor";
  fields_[kCpuInfoModel] = "model name";
  fields_[kCpuInfoHardware] = "Hardware";
  fields_[kCpuInfoFeatures] = "Features";
  fields_[kCpuInfoArchitecture] = "CPU architecture";
  method_ = kCpuInfoSystem;
  ProcCpuInfo::Init();
#elif defined(HOST_ARCH_ARM64) && defined(__FreeBSD__)
  fields_[kCpuInfoProcessor] = "hw.model";
  fields_[kCpuInfoModel] = "hw.model";
  fields_[kCpuInfoHardware] = "hw.model";
  fields_[kCpuInfoFeatures] = nullptr;
  fields_[kCpuInfoArchitecture] = nullptr;
  method_ = kCpuInfoSystem;
#elif defined(HOST_ARCH_ARM64)
  fields_[kCpuInfoProcessor] = "Processor";
  fields_[kCpuInfoModel] = "CPU implementer";
  fields_[kCpuInfoHardware] = "CPU implementer";
  fields_[kCpuInfoFeatures] = "Features";
  fields_[kCpuInfoArchitecture] = "CPU architecture";
  method_ = kCpuInfoSystem;
  ProcCpuInfo::Init();
#elif defined(HOST_ARCH_RISCV32) || defined(HOST_ARCH_RISCV64)
  // We only rely on the base Linux configuration of IMAFDC, so don't need
  // dynamic feature detection.
  method_ = kCpuInfoNone;
#else
#error Unrecognized target architecture
#endif
}

void CpuInfo::Cleanup() {
  if (method_ == kCpuInfoCpuId) {
    CpuId::Cleanup();
  } else if (method_ == kCpuInfoSystem) {
#if !defined(__FreeBSD__)
    ProcCpuInfo::Cleanup();
#endif
  } else {
    ASSERT(method_ == kCpuInfoNone);
  }
}

bool CpuInfo::FieldContains(CpuInfoIndices idx, const char* search_string) {
  if (method_ == kCpuInfoCpuId) {
    const char* field = CpuId::field(idx);
    if (field == nullptr) return false;
    bool contains = (strstr(field, search_string) != nullptr);
    free(const_cast<char*>(field));
    return contains;
  } else if (method_ == kCpuInfoSystem) {
#if defined(__FreeBSD__)
    char* field = ReadSysctlString(FieldName(idx));
    if (field == nullptr) return false;
    bool contains = (strstr(field, search_string) != nullptr);
    free(field);
    return contains;
#else
    return ProcCpuInfo::FieldContains(FieldName(idx), search_string);
#endif
  } else {
    UNREACHABLE();
  }
}

const char* CpuInfo::ExtractField(CpuInfoIndices idx) {
  if (method_ == kCpuInfoCpuId) {
    return CpuId::field(idx);
  } else if (method_ == kCpuInfoSystem) {
#if defined(__FreeBSD__)
    return ReadSysctlString(FieldName(idx));
#else
    return ProcCpuInfo::ExtractField(FieldName(idx));
#endif
  } else {
    UNREACHABLE();
  }
}

bool CpuInfo::HasField(const char* field) {
  if (method_ == kCpuInfoCpuId) {
    return (strcmp(field, fields_[kCpuInfoProcessor]) == 0) ||
           (strcmp(field, fields_[kCpuInfoModel]) == 0) ||
           (strcmp(field, fields_[kCpuInfoHardware]) == 0) ||
           (strcmp(field, fields_[kCpuInfoFeatures]) == 0);
  } else if (method_ == kCpuInfoSystem) {
#if defined(__FreeBSD__)
    return field != nullptr &&
           sysctlbyname(field, nullptr, nullptr, nullptr, 0) == 0;
#else
    return ProcCpuInfo::HasField(field);
#endif
  } else if (method_ == kCpuInfoNone) {
    return false;
  } else {
    UNREACHABLE();
  }
}

}  // namespace dart

#endif  // defined(DART_HOST_OS_LINUX)
