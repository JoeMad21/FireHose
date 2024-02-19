CC = g++

all: firehose

firehose: libipu.a
	$(CC) -fopenmp firehose_main.cpp -L/home/jomad21/myFiles/Tensor_Decomp_Scratch/device_libraries -lipu -lpoplar -lpoplin -lpoputil -lpopops -o firehose
libipu.a: mylib.o
	ar rcs ./device_libraries/libipu.a ./device_libraries/mylib.o
mylib.o:
	popc -o ./device_libraries/io_codelet.gp ./device_libraries/io_codelet.cpp
	$(CC) -c -fopenmp ./device_libraries/firehose_ipu.cpp -o ./device_libraries/mylib.o

clean_app:
	rm firehose

clean_lib:
	rm ./device_libraries/mylib.o

clean_logs:
	rm tensor_decomp_test_*

clean: clean_app clean_lib clean_logs

get:
	git pull

run:
	sbatch demo.batch

super: clean get all run

help:
	echo 'rm firehose\nrm ./device_libraries/mylib.o'
	echo 'rm ./device_libraries/mylib.o'
	echo 'rm tensor_decomp_test_*'
	echo 'git pull'
	echo 'g++ -fopenmp firehose_main.cpp -L/home/jomad21/myFiles/Tensor_Decomp_Scratch/device_libraries -lipu -lpoplar -lpoplin -lpoputil -lpopops -o firehose'
	echo 'ar rcs ./device_libraries/libipu.a ./device_libraries/mylib.o'
	echo 'popc -o ./device_libraries/io_codelet.gp ./device_libraries/io_codelet.cpp'
	echo 'g++ -c -fopenmp ./device_libraries/firehose_ipu.cpp -o ./device_libraries/mylib.o'
	echo 'sbatch demo.batch'

