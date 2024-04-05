SHELL = /bin/sh

compile: compile_client compile_server

compile_client: 
	gcc -L ../../src ./client.c ../../src/*.c -o client.out

compile_server: 
	gcc -L ../../src ./server.c ../../src/*.c -o server.out

run_client: compile_client
	./client.out

run_server: compile_server
	./server.out

debug_client: compile_client
	valgrind  --leak-check=full --track-origins=yes ./client.out

debug_server: compile_server 
	valgrind  --leak-check=full --track-origins=yes ./server.out
