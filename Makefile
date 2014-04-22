all:server client

server:server.o net.o util.o logger.o
	g++ server.o net.o util.o logger.o -o server -lm -lpthread 

client:client.o net.o util.o logger.o
	g++ client.o net.o util.o logger.o -o client

server.o:server.cpp
	g++ -c server.cpp server.h net.h logger.h util.h

client.o:client.cpp
	g++ -c client.cpp client.h net.h logger.h util.h

util.o:util.cpp
	g++ -c util.cpp util.h

net.o:net.cpp
	g++ -c net.cpp net.h logger.h

logger.o:logger.cpp
	g++ -c logger.cpp logger.h

clean:
	rm -f *.o *.h.gch servDebugFile_* heartbeatLog_* logHash* 
	touch *
