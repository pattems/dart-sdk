// Copyright (c) 2026, the Dart project authors.  Please see the AUTHORS file
// for details. All rights reserved. Use of this source code is governed by a
// BSD-style license that can be found in the LICENSE file.

#if !defined(DART_IO_SECURE_SOCKET_DISABLED)

#include "platform/globals.h"
#if defined(DART_HOST_OS_BSD)

#include "bin/security_context.h"

#include <openssl/bio.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>

#include "bin/directory.h"
#include "bin/file.h"
#include "bin/secure_socket_filter.h"
#include "bin/secure_socket_utils.h"
#include "platform/syslog.h"

namespace dart {
namespace bin {

const intptr_t SSLCertContext::kApproximateSize = sizeof(SSLCertContext);

void SSLCertContext::TrustBuiltinRoots() {
  // First, try to use locations specified on the command line.
  if (root_certs_file() != nullptr) {
    LoadRootCertFile(root_certs_file());
    return;
  }
  if (root_certs_cache() != nullptr) {
    LoadRootCertCache(root_certs_cache());
    return;
  }

  if (bypass_trusting_system_roots()) {
    if (SSL_LOG_STATUS) {
      Syslog::Print("Bypass trusting built-in system roots\n");
    }
  } else {
    // As on Linux, we use the compiled-in trusted certs as a last resort.
    // First, we try to find the trusted certs in various standard
    // locations. A good discussion of the complexities of this endeavor
    // can be found here:
    //
    // https://www.happyassassin.net/2015/01/12/a-note-about-ssltls-trusted-certificate-stores-and-platforms/
    //
    // This set of locations was copied from gRPC.
    const char* kCertFiles[] = {
        "/etc/ssl/certs/ca-certificates.crt",
        "/etc/pki/tls/certs/ca-bundle.crt",
        "/etc/ssl/ca-bundle.pem",
        "/etc/pki/tls/cacert.pem",
        "/etc/pki/ca-trust/extracted/pem/tls-ca-bundle.pem",
    };
    const char* kCertDirectories[] = {
        "/etc/ssl/certs",         "/system/etc/security/cacerts",
        "/usr/local/share/certs", "/etc/pki/tls/certs",
        "/etc/openssl/certs",
    };
    for (size_t i = 0; i < ARRAY_SIZE(kCertFiles); i++) {
      const char* bundle = kCertFiles[i];
      if (File::Exists(nullptr, bundle)) {
        LoadRootCertFile(bundle);
        return;
      }
    }
    for (size_t i = 0; i < ARRAY_SIZE(kCertDirectories); i++) {
      const char* cachedir = kCertDirectories[i];
      if (Directory::Exists(nullptr, cachedir) == Directory::EXISTS) {
        LoadRootCertCache(cachedir);
        return;
      }
    }
  }

  // Fall back on the compiled-in certs if the standard locations don't exist.
  if (SSL_LOG_STATUS) {
    Syslog::Print("Trusting compiled-in roots\n");
  }
  AddCompiledInCerts();
}

void SSLCertContext::RegisterCallbacks(SSL* ssl) {
  // No callbacks to register for implementations using BoringSSL's built-in
  // verification mechanism.
}

TrustEvaluateHandlerFunc SSLCertContext::GetTrustEvaluateHandler() {
  return nullptr;
}

}  // namespace bin
}  // namespace dart

#endif  // defined(DART_HOST_OS_BSD)

#endif  // !defined(DART_IO_SECURE_SOCKET_DISABLED)
