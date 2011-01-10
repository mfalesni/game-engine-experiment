#!/usr/bin/env bash

gcc -std=c99 -Wall `pkg-config allegro --libs` -lm -O3 -ffast-math -o test test.c