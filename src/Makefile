client: client.o func_wrapper.o pktlib.o fileio.o signal.o
	gcc -o client client.o func_wrapper.o pktlib.o fileio.o signal.o

nameserver: nameserver.o func_wrapper.o pktlib.o fileio.o signal.o
	gcc -o nameserver nameserver.o func_wrapper.o pktlib.o fileio.o signal.o

routeserver: routeserver.o func_wrapper.o fileio.o signal.o
	gcc -o routeserver routeserver.o func_wrapper.o fileio.o signal.o

client.o: client.c nameservice.h
	gcc -c client.c

nameserver.o: nameserver.c nameservice.h
	gcc -c nameserver.c

routeserver.o: routeserver.c nameservice.h
	gcc -c routeserver.c

func_wrapper.o: func_wrapper.c nameservice.h
	gcc -c func_wrapper.c

pktlib.o: pktlib.c nameservice.h
	gcc -c pktlib.c

fileio.o: fileio.c nameservice.h
	gcc -c fileio.c

signal.o: signal.c nameservice.h
	gcc -c signal.c