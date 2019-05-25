#!/bin/bash
#
# This script build and start the httap
#

cd src &&
gcc -static -DHTTAP_VERBOSE -DSHOW_CLIENT -DHTTAP_SERVE_FILES -DENABLE_LOOPBACK -Wall test_HTTaP.c -o ../test_HTTaP

export HTTAP_KEEPALIVE=15
export HTTAP_ROOTPAGE=example.html
export HTTAP_TCPPORT=60001
export HTTAP_STATICPATH=static
./test_HTTaP
