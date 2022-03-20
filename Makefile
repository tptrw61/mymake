
mymake: mymake.o
	gcc -Wall -o mymake mymake.o
mymake.o: mymake.c
	gcc -Wall -c mymake.c
