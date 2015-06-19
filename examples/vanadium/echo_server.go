// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package main

import (
	"flag"
	"fmt"
	"log"

	"golang.org/x/mobile/app"

	"mojo/public/go/application"
	"mojo/public/go/bindings"
	"mojo/public/go/system"

	"examples/vanadium/vanadium"
	echovdl "examples/vanadium/vdl"

	_ "v.io/x/ref/runtime/factories/generic"

	"v.io/v23"
	"v.io/v23/context"
	"v.io/v23/naming"
	"v.io/v23/rpc"
	"v.io/v23/security"
	"v.io/v23/vom"
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

func (echo *EchoImpl) Echo(ctx *context.T, call rpc.ServerCall, inValue string) (outValue string, err error) {
	log.Printf("ves: echoimpl: echoing %s\n", inValue)
	return inValue, nil
}

type LocalVanadiumServiceImpl struct {
	endpointStr string
	blessings   []byte
}

func (echo *LocalVanadiumServiceImpl) GetEndpoint() (outValue *string, err error) {
	log.Printf("ves: lvs: GetEndpoint() -> %v", echo.endpointStr)
	return &echo.endpointStr, nil
}

func (echo *LocalVanadiumServiceImpl) GetRoot() (outValue []byte, err error) {
	log.Printf("ves: lvs: GetRoot() -> %v", echo.blessings)
	return echo.blessings, nil
}

type EchoServerDelegate struct {
	stubs []*bindings.Stub

	endpoints []naming.Endpoint
	ctx       *context.T
	shutdown  v23.Shutdown
}

func (delegate *EchoServerDelegate) Initialize(context application.Context) {
	log.Printf("ves: initialize")
	delegate.ctx, delegate.shutdown = v23.Init()
	s, err := v23.NewServer(delegate.ctx)
	if err != nil {
		panic(fmt.Sprintf("Error starting server: %v\n", err))
	}
	endpoints, err := s.Listen(v23.GetListenSpec(delegate.ctx))
	if err != nil {
		panic(fmt.Sprintf("Error listening: %v\n", err))
	}
	delegate.endpoints = endpoints

	echoServer := echovdl.EchoServer(&EchoImpl{})

	if err := s.Serve("", echoServer, security.AllowEveryone()); err != nil {
		panic(fmt.Sprintf("error serving: %v", err))
	}
	log.Printf("ves: initialize: done")
}

func (delegate *EchoServerDelegate) Create(request vanadium.LocalVanadiumService_Request) {
	log.Printf("ves: create")
	blessings := v23.GetPrincipal(delegate.ctx).BlessingStore().Default()
	bytes, err := vom.Encode(blessings)
	if err != nil {
		panic(fmt.Sprintf("Error encoding blessings: %v", err))
	}

	stub := vanadium.NewLocalVanadiumServiceStub(request, &LocalVanadiumServiceImpl{
		delegate.endpoints[0].String(),
		bytes,
	}, bindings.GetAsyncWaiter())
	delegate.stubs = append(delegate.stubs, stub)
	go func() {
		for {
			if err := stub.ServeRequest(); err != nil {
				connectionError, ok := err.(*bindings.ConnectionError)
				if !ok || !connectionError.Closed() {
					log.Printf("ves: error: %v", err)
				} else {
					log.Printf("ves: ok %v connection closed %v", ok, connectionError.Closed())
				}
				break
			}
		}
	}()
	log.Printf("ves: create: done")
}

func (delegate *EchoServerDelegate) AcceptConnection(connection *application.Connection) {
	log.Printf("ves: accept")
	connection.ProvideServices(&vanadium.LocalVanadiumService_ServiceFactory{delegate})
	log.Printf("ves: accept: done")
}

func (delegate *EchoServerDelegate) Quit() {
	log.Printf("ves: quit")
	delegate.shutdown()
}

//export MojoMain
func MojoMain(handle C.MojoHandle) C.MojoResult {
	application.Run(&EchoServerDelegate{}, system.MojoHandle(handle))
	return C.MOJO_RESULT_OK
}

func main() {
	app.Run(app.Callbacks{})
}
