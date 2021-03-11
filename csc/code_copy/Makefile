all: fibo.o main.o
fibo.o: fibo.c
	gcc -c -I../include fibo.c
main.o: main.c
	gcc -c -I../include  main.c
clean:
	rm *.o
