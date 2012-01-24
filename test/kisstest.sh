#!/bin/sh

echo "output to decoded.kss"

gst-launch filesrc location=$1 ! \
    kissnrzi ! application/x-raw-float,rate=1200 ! \
    bpskrcmod ! audio/x-complex-float,rate=44100 ! \
    iqpolar ! bpskrcdem ! application/x-raw-float,rate=1200 ! \
    nrzikiss ! filesink location=decoded.kss
