#!/bin/sh

brew update
brew outdated cmake || brew upgrade cmake
brew outdated pkgconfig || brew upgrade pkgconfig
