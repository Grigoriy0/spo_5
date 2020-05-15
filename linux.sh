#!/bin/bash
echo -e "\e[36mBuid lib:\e[39m"
cd lib
./build.sh
if [ "$?" -eq "1" ]; then
    echo -e "\e[31mfailed\e[39m"
    exit 1
fi
cd ..


echo -e "\e[36mBuild project:\e[39m"
cd linux
#g++ -o App linux/main.cpp -lpthread -ldl -lrt
make all
if [ "$?" -eq "1" ]; then
    echo -e "\e[31mfailed\e[39m"
    exit 1
fi


echo -e "\e[36mRun:\e[39m"
./App
if [ "$?" -eq "0" ]; then
    echo -e "\e[32mProgram successfully completed\e[39m"
else
    echo -e "\e[31mProject exited with code $?\e[39m"
fi

