#!/bin/sh

if [ "${ANALYZE}" ]; then
  echo "yes" | sudo add-apt-repository "deb http://llvm.org/apt/`lsb_release -sc`/ llvm-toolchain-`lsb_release -sc`-3.6 main"
  wget -O - http://llvm.org/apt/llvm-snapshot.gpg.key|sudo apt-key add -
fi

echo "yes" | sudo add-apt-repository "deb http://archive.ubuntu.com/ubuntu `lsb_release -sc` main universe restricted multiverse"
echo "yes" | sudo apt-add-repository ppa:psi29a/`lsb_release -sc`
sudo apt-get update -qq
sudo apt-get install -qq libgtest-dev google-mock
sudo apt-get install -qq libopenal-dev
if [ "${ANALYZE}" ]; then sudo apt-get install -qq clang-3.6; fi
sudo mkdir /usr/src/gtest/build
cd /usr/src/gtest/build
sudo cmake ..
sudo make -j4
sudo ln -s /usr/src/gtest/build/libgtest.so /usr/lib/libgtest.so
sudo ln -s /usr/src/gtest/build/libgtest_main.so /usr/lib/libgtest_main.so
