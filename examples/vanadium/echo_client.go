// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package main

import (
	"flag"
	"log"

	"v.io/v23"
	"v.io/v23/context"
	"v.io/v23/security"
	"v.io/v23/vom"

	"golang.org/x/mobile/app"

	"mojo/public/go/application"
	"mojo/public/go/bindings"
	"mojo/public/go/system"

	echovdl "examples/vanadium/vdl"

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

type VanadiumClientImpl struct {
	ctx         *context.T
	endpointStr string
}

func (impl *VanadiumClientImpl) EchoOverVanadium(inValue *string) (outValue *string, err error) {
	log.Printf("Echoing to %s...\n", impl.endpointStr)
	c := echovdl.EchoClient("/" + impl.endpointStr)
	out, err := c.Echo(impl.ctx, "TestMessage")
	if err != nil || out != "TestMessage" {
		log.Fatalf("Failed to echo. Message: %s. Error: %v.\n", out, err)
	}

	log.Printf("Result of echo: %s\n", out)
	return &out, nil
}

type EchoClientDelegate struct {
	stubs []*bindings.Stub

	shutdown v23.Shutdown

	ctx         *context.T
	endpointStr string
}

func (delegate *EchoClientDelegate) Initialize(mctx application.Context) {
	log.Printf("vec: initializing")
	delegate.ctx, delegate.shutdown = v23.Init()

	blessingsBytes := GetRoot(mctx)
	var blessings security.Blessings
	if err := vom.Decode(blessingsBytes, &blessings); err != nil {
		log.Fatalf("Error decoding blessings: %v", err)
	}
	v23.GetPrincipal(delegate.ctx).AddToRoots(blessings)
	log.Printf("vec: initializing: Added %v to roots\n", blessings)

	delegate.endpointStr = GetEndpoint(mctx)

	log.Printf("vec: initializing: done")
}

func GetEndpoint(ctx application.Context) string {
	vanadiumRequest, vanadiumPointer := vanadium.CreateMessagePipeForLocalVanadiumService()
	ctx.ConnectToApplication("mojo:vanadium_echo_server").ConnectToService(&vanadiumRequest)
	vanadiumProxy := vanadium.NewLocalVanadiumServiceProxy(vanadiumPointer, bindings.GetAsyncWaiter())
	response, err := vanadiumProxy.GetEndpoint()
	if err != nil {
		log.Fatalf("Error getting endpoint: %v\n", err)
	}
	vanadiumProxy.Close_Proxy()

	return *response
}

func GetRoot(ctx application.Context) []byte {
	vanadiumRequest, vanadiumPointer := vanadium.CreateMessagePipeForLocalVanadiumService()
	ctx.ConnectToApplication("mojo:vanadium_echo_server").ConnectToService(&vanadiumRequest)
	vanadiumProxy := vanadium.NewLocalVanadiumServiceProxy(vanadiumPointer, bindings.GetAsyncWaiter())
	rootBytes, err := vanadiumProxy.GetRoot()
	if err != nil {
		log.Fatalf("Error getting root: %v", err)
	}
	vanadiumProxy.Close_Proxy()

	return rootBytes
}

func (delegate *EchoClientDelegate) Create(request vanadium.VanadiumClient_Request) {
	log.Printf("vec: create")
	stub := vanadium.NewVanadiumClientStub(request, &VanadiumClientImpl{
		delegate.ctx,
		delegate.endpointStr,
	}, bindings.GetAsyncWaiter())
	delegate.stubs = append(delegate.stubs, stub)
	go func() {
		for {
			if err := stub.ServeRequest(); err != nil {
				connectionError, ok := err.(*bindings.ConnectionError)
				if !ok || !connectionError.Closed() {
					log.Printf("vec: error: %v", err)
				} else {
					log.Printf("vec: ok %v connection closed %v", ok, connectionError.Closed())
				}
				break
			}
		}
	}()
	log.Printf("vec: create: done")
}

func (delegate *EchoClientDelegate) AcceptConnection(connection *application.Connection) {
	log.Printf("vec: accept")
	connection.ProvideServices(&vanadium.VanadiumClient_ServiceFactory{delegate})
	log.Printf("vec: accept: done")
}

func (delegate *EchoClientDelegate) Quit() {
	log.Printf("vec: quit")
}

//export MojoMain
func MojoMain(handle C.MojoHandle) C.MojoResult {
	application.Run(&EchoClientDelegate{}, system.MojoHandle(handle))
	return C.MOJO_RESULT_OK
}

func main() {
	app.Run(app.Callbacks{})
}
