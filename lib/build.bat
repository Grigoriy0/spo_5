g++ -shared -o liblib.dll library.cpp
echo build finished
COPY liblib.dll ..\windows\
