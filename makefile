DSGD.exe:dsgd.o main.o
	mpicc $(CFLAGS) -o DSGD.exe dsgd.o main.o
dsgd.o:dsgd.c dsgd.h
	mpicc $(CFLAGS) -c dsgd.c
main.o:main.c dsgd.h
	mpicc $(CFLAGS) -c main.c

clean:
	rm *.o -f
