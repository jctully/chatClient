all: prog3_server prog3_observer prog3_participant

prog3_server: prog3_server.c
	gcc -Wall prog3_server.c -o prog3_server

prog3_observer: prog3_observer.c
	gcc -Wall prog3_observer.c -o prog3_observer

prog3_participant: prog3_participant.c
	gcc -Wall prog3_participant.c -o prog3_participant
