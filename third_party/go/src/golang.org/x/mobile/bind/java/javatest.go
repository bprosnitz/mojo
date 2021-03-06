// Copyright 2014 The Go Authors. All rights reserved.
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

//+build ignore

package main

import (
	"golang.org/x/mobile/app"
	"golang.org/x/mobile/bind/java"

	_ "golang.org/x/mobile/bind/java/testpkg/go_testpkg"
)

func main() {
	app.Run(app.Callbacks{Start: java.Init})
}
