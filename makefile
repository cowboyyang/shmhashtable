CC=g++
object = Main.o
Main : ${object}
	${CC} -o Main ${object}
Main.o : Main.cpp MShmHashTable.hpp
	${CC} -c Main.cpp -Wno-deprecated

clean:
	rm -f ${object}
	rm -f Main
