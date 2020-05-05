
all: mandel mandelmovie

mandel: mandel.o bitmap.o
	gcc mandel.o bitmap.o -o mandel -lpthread -std=c99

mandelmovie: mandelmovie.c
	gcc -Wall -o mandelmovie mandelmovie.c -lm -std=c99

mandel.o: mandel.c
	gcc -Wall -g -c mandel.c -o mandel.o -std=c99

bitmap.o: bitmap.c
	gcc -Wall -g -c bitmap.c -o bitmap.o

clean:
	rm -f mandel.o bitmap.o mandel
