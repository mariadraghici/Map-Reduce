build: tema1

tema1: tema1.cpp
	g++ tema1.cpp -o tema1 -Wall -Werror -lpthread

clean:
	rm -f tema1