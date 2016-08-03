#!/bin/sh

brew update
brew rm cmake || true
brew rm pkgconfig || true
brew install cmake pkgconfig
