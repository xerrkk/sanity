#!/bin/sh
cc -static -o /sbin/sanity sanity.c
cc -o alps pkg/alps.c
