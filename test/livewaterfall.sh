#!/bin/sh

gst-launch osssrc ! "audio/x-raw-int,rate=44100,width=16" \
! audioconvert ! iqcmplx ! cmplxfft ! waterfall ! xvimagesink
