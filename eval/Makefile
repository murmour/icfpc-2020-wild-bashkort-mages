CC = g++
CFLAGS = -O2 -Wall -std=c++17

.PHONY: eval eval_release


eval:
	$(CC) $(CFLAGS) -o main.o -c main.cpp
	$(CC) $(CFLAGS) -o bot.o -c bot.cpp -DMAIN
	$(CC) $(CFLAGS) main.o bot.o -o eval

eval_release:
	$(CC) $(CFLAGS) -o main.o -c main.cpp -DHTTPLIB -DSUBMISSION
	$(CC) $(CFLAGS) -o bot.o -c bot.cpp -DMAIN
	$(CC) $(CFLAGS) main.o bot.o -o eval_rel
	cp eval_rel ../../submissions/app/app
	strip ../../submissions/app/app

alien: alien.ml
	ocamlfind opt -linkpkg -package curl alien.ml -o alien

clean:
	rm -rf *.o
