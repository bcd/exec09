#!/bin/sh
sudo port install readline autoconf automake
automake --add-missing
autoreconf --force
./configure --enable-readline --enable-6309 --libdir=/opt/local/lib --prefix=/opt/gcc6809
make
sudo make install
