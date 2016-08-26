#
include Makefile.inc
# Subdirectories
TEST_DIR = tests
OBJ_DIR = /home/rku000/source/2dads/obj/

.PHONY: clean dist

#all: output.o initialize.o diagnostics.o diagnostics_cu.o slab_config.o slab_cuda.o shader.o

DEFINES	= -DPINNED_HOST_MEMORY -DBOOST_NOINLINE='__attribute__ ((noinline))' 

#SOURCES_CPP=shader.cpp slab_config.cpp output.cpp slab_config.cpp
#SOURCES_CU=initialize.cu slab_cuda.cu

#OBJECTS=$(OBJ_DIR)/slab_config.o $(OBJ_DIR)/output.o $(OBJ_DIR)/diagnostics_cu.o $(OBJ_DIR)/initialize.o $(OBJ_DIR)/slab_cuda.o

#shader.o: shader.cpp
#	$(CC) $(CFLAGS) $(DEFINES) -c -o $(OBJ_DIR)/shader.o shader.cpp $(INCLUDES)
#
slab_config.o: slab_config.cpp include/slab_config.h
	$(CC) $(CFLAGS) $(DEFINES) -c -o $(OBJ_DIR)/slab_config.o slab_config.cpp $(INCLUDES)

output.o: output.cpp include/output.h
	$(CC) $(CFLAGS) $(DEFINES) -c -o $(OBJ_DIR)/output.o output.cpp $(INCLUDES)

slab_bc_host.o: slab_bc.cpp
	$(CC) $(CFLAGS) $(DEFINES) -DHOST -c -o $(OBJ_DIR)/slab_bc_host.o slab_bc.cpp $(INCLUDES)

slab_bc_devive.o: slab_bc.cpp
	$(CUDACC) $(CUDAFLAGS) $(DEFINES) -DHOST -c -o $(OBJ_DIR)/slab_bc_host.o slab_bc.cpp $(INCLUDES)

#diagnostics.o: diagnostics.cpp include/diagnostics.h
#	$(CC) $(CFLAGS) $(DEFINES) -c -o $(OBJ_DIR)/diagnostics.o diagnostics.cpp $(INCLUDES)
#
#diagnostics_cu.o: diagnostics_cu.cu include/diagnostics_cu.h include/cuda_darray.h include/cuda_array4.h
#	$(CUDACC) $(CUDACFLAGS) $(DEFINES) -c -o $(OBJ_DIR)/diagnostics_cu.o diagnostics_cu.cu $(INCLUDES)
#
#initialize.o: initialize.cu include/initialize.h
#	$(CUDACC) $(CUDACFLAGS) $(DEFINES) -c -o $(OBJ_DIR)/initialize.o initialize.cu $(INCLUDES)
#
#slab_cuda.o: slab_cuda.cu include/slab_cuda.h include/cuda_array4.h
#	$(CUDACC) $(CUDACFLAGS) $(DEFINES) -c -o $(OBJ_DIR)/slab_cuda.o slab_cuda.cu $(INCLUDES)
#
#2dads: slab_config.o output.o diagnostics_cu.o initialize.o slab_cuda.o
#	$(CC) $(CFLAGS) -o run/2dads $(OBJECTS) main3.cpp $(INCLUDES) $(LFLAGS) 
#
#2dads_profile: slab_config.o initialize.o slab_cuda.o
#	$(CC) $(CFLAGS) -o run/2dads_profile $(OBJECTS) main.cpp $(INCLUDES) $(LFLAGS) 
#	#$(CUDACC) $(CUDACFLAGS) -o run/2dads_profile $(OBJECTS) main.cpp $(INCLUDES) $(LFLAGS) 

2dads_bc_host: slab_config.o output.o slab_bc_host.o
	$(CC) $(CFLAGS) -DHOST -o run_bc/2dads_bc $(OBJECTS) main_bc.cpp $(INCLUDES) $(LFLAGS)

2dads_bc_device: slab_config.o output.o slab_bc_device.o
	$(CUDACC) $(CFLAGS) -DDEVICE -o run_bc/2dads_bc_device $(OBJECTS) main_bc.cu

#tests: cuda_array2 slab_cuda initialize output diagnostics
#	$(MAKE) -C $(TEST_DIR)

# PIC objects
#pic_objs: $(SOURCES_CPP) $(SOURCES_CU)
#	$(CC) $(CFLAGS) $(DEFINES) -fPIC -c -o $(OBJ_DIR)/slab_config_pic.o slab_config.cpp $(INCLUDES)
#	$(CC) $(CFLAGS) $(DEFINES) -fPIC -c -o $(OBJ_DIR)/output_pic.o output.cpp $(INCLUDES)
#	$(CC) $(CFLAGS) $(DEFINES) -fPIC -c -o $(OBJ_DIR)/diagnostics_pic.o diagnostics.cpp $(INCLUDES)
#	$(CUDACC) $(CUDACFLAGS) $(DEFINES) -Xcompiler -fPIC -c -o $(OBJ_DIR)/initialize_pic.o initialize.cu $(INCLUDES)
#	$(CUDACC) $(CUDACFLAGS) $(DEFINES) -Xcompiler -fPIC -c -o $(OBJ_DIR)/slab_cuda_pic.o slab_cuda.cu $(INCLUDES)


dist:
	rm -i dist/*.cpp
	rm -i dist/*.cu
	rm -i dist/include/*.h
	cp -R *.cpp *.cu dist
	cp Makefile dist/Makefile_nemesis
	cp Makefile.inc dist/Makefile_nemesis.inc
	cp include/*.h dist/include/
	tar cvfj 2dads-cuda-`date +%Y-%m-%d`.tar.bz2 dist/*

clean:
	rm obj/*.o

