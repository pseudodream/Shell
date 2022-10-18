
output: shell.o
	gcc shell.o -o shell

shell.o: shell.c
	gcc -c shell.c

clean: 
	rm *.o output:
