#!/bin/sh
cc -o /sbin/gni gni.c $(pkg-config --cflags --libs guile-3.0)
cc -static -o /sbin/insomnia insomnia.c
