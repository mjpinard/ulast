#

CC = cc -Wall -Wextra -g

ulast: ulast.o dumputmp.o
	$(CC)  ulast.o dumputmp.o -o ulast

ulast.o: ulast.c dumputmp.h
	$(CC) -c ulast.c

dumputmp.o: dumputmp.c dumputmp.h
	$(CC) -c dumputmp.c

clean:
	rm -f *.o more03