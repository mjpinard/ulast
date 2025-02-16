#

CC = cc -Wall -Wextra -g

ulast: ulast.o bufferlib.o
	$(CC)  ulast.o bufferlib.o -o ulast

ulast.o: ulast.c bufferlib.h
	$(CC) -c ulast.c

bufferlib.o: bufferlib.c bufferlib.h
	$(CC) -c bufferlib.c

clean:
	rm -f *.o ulast