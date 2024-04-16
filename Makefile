CC = gcc
CFLAGS = -Wall -g

myshell: myshell.c LineParser.c LineParser.h
	$(CC) $(CFLAGS) -o myshell myshell.c LineParser.c


mypipeline: mypipeline.c
	gcc -Wall -g -o mypipeline mypipeline.c


clean:
	rm -f myshell mypipeline
	
	