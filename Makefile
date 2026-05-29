CC = gcc
FLAGS = -Wall -Wextra -O2

all: mauvyd mauvyctl

mauvyd:
	$(CC) $(FLAGS) mauvyd.c mauvyd-headers/pcg.c -Imauvyd-headers -o mauvyd -lpthread

mauvyctl:
	$(CC) $(FLAGS) mauvyctl.c -o mauvyctl

clean:
	rm -rf mauvyctl mauvyd
