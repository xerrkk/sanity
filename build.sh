#!/bin/sh
cc -o /sbin/sherpa sherpa.c $(pkg-config --cflags --libs guile-3.0)
cc -static -o /sbin/sherpactl sherpactl.c
