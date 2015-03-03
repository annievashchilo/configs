all: dras6
CFLAGS= -Wpacked -Wall -W -Wmissing-prototypes -Wno-main -Wno-unused-parameter -Wno-unused-value -Wno-sign-compare
dras6: dras6.o dhcp.h
dras6raw: dras6raw.o rawnet.o
dras6:
	gcc ${CFLAGS}  -o dras6 dras6.o
dras6raw:
	gcc ${CFLAGS}  -DPCAP=1 -o dras6raw dras6raw.o rawnet.o -ldnet -lpcap
dras6raw.o: dras6.c dhcp.h
dras6raw.o:
	gcc ${CFLAGS} -DPCAP=1 -c -o dras6raw.o dras6.c
.c.o:
	${CC} ${CFLAGS} -c $<
clean:
	rm -f dras6 dras6raw *.o
