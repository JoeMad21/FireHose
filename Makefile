CC = g++

all: firehose

firehose: libipu.a
		$(CC) firehose_main.cpp -L/home/jomad21/myFiles/Tensor_Decomp_Scratch/device_libraries -lipu -lpoplar -lpoplin -lpoputil -o firehose
libipu.a: mylib.o
        ar rcs libipu.a mylib.o
mylib.o: firehose_ipu.cpp
		popc -o io_codelet.o io_codelet.cpp
        $(CC) -c firehose_ipu.cpp -o mylib.o