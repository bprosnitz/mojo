// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

library apptest;

import 'dart:async';
import 'mojo:application';
import 'mojo:bindings';
import 'mojo:core';

// Import and reexport the unittest package. We are a *.dartzip file designed to
// be linked into your_apptest.mojo file and are your main entrypoint.
import 'package:unittest/unittest.dart';
export 'package:unittest/unittest.dart';

// This class is an application that does nothing but tears down the connections
// between each test.
class _ConnectionToShellApplication extends Application {
  final List<Function> _testFunctions;

  _ConnectionToShellApplication.fromHandle(MojoHandle handle,
      this._testFunctions)
      : super.fromHandle(handle);

  // Only run the test suite passed in once we have received an initialize()
  // call from the shell. We need to first have a valid connection to the shell
  // so that apptests can connect to other applications.
  void initialize(List<String> args, String url) {
    _testFunctions.forEach((f) => f(this));
  }
}

// A configuration which properly shuts down our application at the end.
class _CleanShutdownConfiguration extends SimpleConfiguration {
  final _ConnectionToShellApplication _application;

  _CleanShutdownConfiguration(this._application) : super() {}

  Duration timeout = const Duration(seconds: 10);

  void onTestResult(TestCase externalTestCase) {
    super.onTestResult(externalTestCase);
    _application.resetConnections();
  }

  void onDone(bool success) {
    _application.close();
    super.onDone(success);
  }
}

// The public interface to apptests.
//
// In a dart mojo application, |incoming_handle| is args[0]. |testFunction| is a
// list of functions that actually contains your testing code, and will pass
// back an application to each of them.
runAppTests(var incomingHandle, List<Function> testFunction) {
  var appHandle = new MojoHandle(incomingHandle);
  var application =
      new _ConnectionToShellApplication.fromHandle(appHandle, testFunction);
  unittestConfiguration = new _CleanShutdownConfiguration(application);
}
