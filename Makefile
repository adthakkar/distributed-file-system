all:server

server:server.o net.o logger.o
	g++ server.o net.o logger.o -o server -lm -lpthread 

server.o:server.cpp
	g++ -c server.cpp server.h net.h logger.h util.h

net.o:net.cpp
	g++ -c net.cpp net.h logger.h

logger.o:logger.cpp
	g++ -c logger.cpp logger.h

clean:
	rm -f *.o *.h.gch servDebugFile_* 
	touch *
