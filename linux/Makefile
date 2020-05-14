FLAGS = -lpthread -ldl -lrt

all: main.cpp
	echo "" > out.txt 
	g++ main.cpp -o App $(FLAGS)
start: App
	./App

clean:
	rm -f App
