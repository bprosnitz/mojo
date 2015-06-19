// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package main

import (
	"fmt"
	"log"

	"golang.org/x/mobile/app"

	"mojo/public/go/application"
	"mojo/public/go/bindings"
	"mojo/public/go/system"

	"examples/vanadium/vanadium"
)

//#include "mojo/public/c/system/types.h"
import "C"

type EchoClientDelegate struct{}

func (delegate *EchoClientDelegate) Initialize(ctx application.Context) {
	log.Printf("eov: initializing")
	echoRequest, echoPointer := vanadium.CreateMessagePipeForVanadiumClient()
	ctx.ConnectToApplication("mojo:basic_echo_client").ConnectToService(&echoRequest)
	echoProxy := vanadium.NewVanadiumClientProxy(echoPointer, bindings.GetAsyncWaiter())
	log.Printf("eov: calling echo over vanadium...\n")
	response, err := echoProxy.EchoOverVanadium(bindings.StringPointer("Hello, Go world!"))
	log.Printf("eov: done with call to echo over vanadium\n")
	if response != nil {
		fmt.Printf("eov: %s\n", *response)
	} else {
		log.Printf("eov: e failed: %v\n", err)
	}
	echoProxy.Close_Proxy()
	ctx.Close()
	log.Printf("eov: initializing: done")
}

func (delegate *EchoClientDelegate) AcceptConnection(connection *application.Connection) {
	connection.Close()
}

func (delegate *EchoClientDelegate) Quit() {
}

//export MojoMain
func MojoMain(handle C.MojoHandle) C.MojoResult {
	application.Run(&EchoClientDelegate{}, system.MojoHandle(handle))
	return C.MOJO_RESULT_OK
}

func main() {
	app.Run(app.Callbacks{})
}
