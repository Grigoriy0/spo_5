#!/bin/bash

g++ --shared -o liblib.so library.cpp
cp liblib.so ../linux/
