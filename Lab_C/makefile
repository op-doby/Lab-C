myshell: myshell.o LineParser.o 
	gcc -g -m32 -Wall  -o myshell myshell.o LineParser.o 
mypipeline: mypipeline.o 
	gcc -g -m32 -Wall  -o mypipeline mypipeline.o 
mypipeline.o: mypipeline.c 
	gcc -m32 -g -Wall -c -o mypipeline.o mypipeline.c 
looper: looper.o
	gcc -g -m32 -Wall -o looper looper.o
myshell.o: myshell.c LineParser.h
	gcc -m32 -g -Wall -c -o myshell.o myshell.c 
LineParser.o: LineParser.c LineParser.h
	gcc -m32 -g -Wall -c -o LineParser.o LineParser.c 
looper.o: Looper.c 
	gcc -m32 -g -Wall -c -o looper.o Looper.c 
.PHONY: clean 
clean: 
	rm -f *.o myshell mypipeline