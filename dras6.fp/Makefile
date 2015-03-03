all: dras6
CFLAGS= -Wpacked -Wall -W -Wmissing-prototypes -Wno-main -Wno-unused-parameter -Wno-unused-value -Wno-sign-compare
dras6: dras6.o dhcp.h dras6.h
dras6:
	gcc ${CFLAGS}  -o dras6 dras6.o
.c.o:
	${CC} ${CFLAGS} -c $<
clean:
	rm -f dras6 *.o
