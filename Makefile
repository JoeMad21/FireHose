CC = g++

all: firehose

firehose: libipu.a
	$(CC) firehose_main.cpp -L/home/jomad21/myFiles/Tensor_Decomp_Scratch/device_libraries -lipu -lpoplar -lpoplin -lpoputil -lpopops -o firehose
libipu.a: mylib.o
	ar rcs ./device_libraries/libipu.a ./device_libraries/mylib.o
mylib.o:
	popc -o ./device_libraries/io_codelet.gp ./device_libraries/io_codelet.cpp
	$(CC) -c ./device_libraries/firehose_ipu.cpp -o ./device_libraries/mylib.o

clean_app:
	rm firehose

clean_lib:
	rm mylib.o

clean_logs:
	rm tensor_decomp_test_*

clean:
	rm firehose
	rm mylib.o
	rm tensor_decomp_test_*