// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package main

import (
	"flag"
	"log"

	"golang.org/x/mobile/app"

	"mojo/public/go/application"
	"mojo/public/go/bindings"
	"mojo/public/go/system"

	"examples/vanadium/vanadium"

	_ "v.io/x/ref/runtime/factories/generic"
)

//#include "mojo/public/c/system/types.h"
import "C"

func init() {
	var x string
	flag.StringVar(&x, "child-connection-id", "", "")
	flag.StringVar(&x, "mojo-platform-channel-handle", "", "")
	flag.StringVar(&x, "url-mappings", "", "")
}

type EchoImpl struct{}

func (echo *EchoImpl) EchoOverVanadium(inValue *string) (outValue *string, err error) {
	log.Printf("goclient: echo %s\n", *inValue)
	return inValue, nil
}

type EchoServerDelegate struct {
	stubs []*bindings.Stub
}

func (delegate *EchoServerDelegate) Initialize(context application.Context) {
	log.Printf("goclient: initialized")
}

func (delegate *EchoServerDelegate) Create(request vanadium.VanadiumClient_Request) {
	log.Printf("goclient: create")
	stub := vanadium.NewVanadiumClientStub(request, &EchoImpl{}, bindings.GetAsyncWaiter())
	delegate.stubs = append(delegate.stubs, stub)
	go func() {
		for {
			log.Printf("goclient: waiting for request")
			if err := stub.ServeRequest(); err != nil {
				log.Printf("goclient: done waiting for request")
				connectionError, ok := err.(*bindings.ConnectionError)
				if !ok || !connectionError.Closed() {
					log.Printf("error getting connection: %v", err)
				}
				break
			}
		}
	}()
	log.Printf("goclient: create: done")
}

func (delegate *EchoServerDelegate) AcceptConnection(connection *application.Connection) {
	log.Printf("goclient: accept")
	connection.ProvideServices(&vanadium.VanadiumClient_ServiceFactory{delegate})
	log.Printf("goclient: accept:done")
}

func (delegate *EchoServerDelegate) Quit() {
	log.Printf("goclient: quit")
	for _, stub := range delegate.stubs {
		stub.Close()
	}
}

//export MojoMain
func MojoMain(handle C.MojoHandle) C.MojoResult {
	application.Run(&EchoServerDelegate{}, system.MojoHandle(handle))
	return C.MOJO_RESULT_OK
}

func main() {
	app.Run(app.Callbacks{})
}
