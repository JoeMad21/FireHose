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

reset:
	for file in ./results*; do : > "$file"; done

help:
	printf "\nrm firehose\nrm ./device_libraries/mylib.o\nrm tensor_decomp_test_*\ngit pull\ng++ -fopenmp firehose_main.cpp -L/home/jomad21/myFiles/Tensor_Decomp_Scratch/device_libraries -lipu -lpoplar -lpoplin -lpoputil -lpopops -o firehose\nar rcs ./device_libraries/libipu.a ./device_libraries/mylib.o\npopc -o ./device_libraries/io_codelet.gp ./device_libraries/io_codelet.cpp'\ng++ -c -fopenmp ./device_libraries/firehose_ipu.cpp -o ./device_libraries/mylib.o\nsbatch demo.batch\n"

