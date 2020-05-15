#!/bin/bash

g++ --shared -o liblib.so library.cpp -fPIC -lrt
cp liblib.so ../linux/
