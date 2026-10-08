// Copyright (c) 2023, the Dart project authors.  Please see the AUTHORS file
// for details. All rights reserved. Use of this source code is governed by a
// BSD-style license that can be found in the LICENSE file.

import 'dart:ffi';
import 'dart:io';

import 'package:expect/expect.dart';

import 'dylib_utils.dart';

void main() {
  testDoesNotExist();
}

void testDoesNotExist() {
  final exception = Expect.throws<ArgumentError>(
    () => DynamicLibrary.open(dylibName('doesnotexist1234')),
  );

  if (Platform.isWindows) {
    Expect.contains(
      'The specified module could not be found.',
      exception.message,
    );
    Expect.contains('(error code: 126)', exception.message);
  } else if (Platform.isLinux) {
    // The wording depends on the C library: glibc says "cannot open shared
    // object file: No such file or directory", musl "Error loading shared
    // library ...: No such file or directory", FreeBSD's rtld "Shared
    // object ... not found".
    Expect.contains('libdoesnotexist1234.so', exception.message);
    Expect.containsAny([
      'No such file or directory',
      'not found',
    ], exception.message);
  } else if (Platform.isMacOS) {
    Expect.contains('libdoesnotexist1234.dylib', exception.message);
    Expect.containsAny(['no such file', 'image not found'], exception.message);
  }
}
