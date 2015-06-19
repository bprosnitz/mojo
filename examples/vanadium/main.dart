// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be

import 'dart:async';

import 'package:mojo/application.dart';
import 'package:mojo/bindings.dart';
import 'package:mojo/core.dart';
import 'package:mojom/mojo/examples/vanadium.mojom.dart';

class VanadiumClientApp extends Application {
  final VanadiumClientProxy vclient = new VanadiumClientProxy.unbound();

  VanadiumClientApp.fromHandle(MojoHandle handle) : super.fromHandle(handle);

  Future initialize(List<String> args, String url) async {
    print("app initialize");
    connectToService("mojo:vanadium_echo_client", vclient);
    print("done with connect call");
    try {
      final VanadiumClientEchoOverVanadiumResponseParams result = await vclient.ptr.echoOverVanadium("Hello world");
      print('Result: ' + result.toString());
    } catch(e) {
      print('Error echoing: ' + e.toString());
    }
    vclient.close();
    close();
  }
}

main(List args) {
  print("main called");
  MojoHandle appHandle = new MojoHandle(args[0]);
  new VanadiumClientApp.fromHandle(appHandle);
}
