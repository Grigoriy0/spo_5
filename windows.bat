@echo off
echo 1. creating dll library
cd lib
call build.bat
cd ..
echo 2. run project
cd windows
call buildandrun.bat
cd ..