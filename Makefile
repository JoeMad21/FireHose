CC = g++

all: firehose

firehose: libipu.a
	$(CC) firehose_main.cpp -L/home/jomad21/myFiles/Tensor_Decomp_Scratch/device_libraries -lipu -lpoplar -lpoplin -lpoputil -o firehose
libipu.a: mylib.o
	ar rcs ./device_libraries/libipu.a ./device_libraries/mylib.o
mylib.o:
	popc -o ./device_libraries/io_codelet.o ./device_libraries/io_codelet.cpp
	$(CC) -c ./device_libraries/firehose_ipu.cpp -o ./device_libraries/mylib.o