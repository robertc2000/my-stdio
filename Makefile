build:
	gcc -c so_stdio.c
	gcc -shared so_stdio.o -o libso_stdio.so

b2:
	gcc -o so_stdio -ggdb so_stdio.c

clean:
	rm -f *.o so_stdio