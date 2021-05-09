CXX=g++
#CXXFLAGS=-std=c++11 -ggdb
CXXFLAGS=-std=c++11 -o3 -ggdb -fno-stack-protector
LDFLAGS=-I./ -I../src -I../motivation/pmemkv-0.3x-vanilla/src
LDFLAGS+=-pthread -lpmem -luuid -lstdc++fs
LDFLAGS+=-L../motivation/pmemkv-0.3x-vanilla/bin -lpmemkv
COMMON=$(wildcard common/*.cpp common/*.hpp)
CODE=$(wildcard volatile/*.hpp persistent/*.hpp recovery/*.hpp)
LIB=../src/libsavitar.a
TARGET=benchmark

all: $(TARGET)

ifeq ($(MODE),rocksdb)
ifeq ($(BENCH),pronto)
CXXFLAGS+=-DPRONTO
LDFLAGS+=-Irocksdb/pronto/include -Lrocksdb/pronto
else
ifeq ($(BENCH),sync)
CXXFLAGS+=-DROCKSDB_SYNC
else
CXXFLAGS+=-DROCKSDB_ASYNC
endif
LDFLAGS+=-Irocksdb/vanilla/include -Lrocksdb/vanilla
endif # BENCH -- pronto
LDFLAGS+=-lrocksdb -ljemalloc -lz
CXXFLAGS+=-DROCKSDB
endif # MODE

ifeq ($(BENCHMARK),nvhash-map-multi)
CXXFLAGS+=-DMULTI_OBJECT
endif

LIBPMEMKV:=$(abspath ../motivation/pmemkv-0.3x-vanilla/bin/libpmemkv.so)

$(LIBPMEMKV):
	(cd ../motivation; BUILD_PMEMKV=pmemkv-0.3x-vanilla ./run-throughput.sh)

$(TARGET): main.cpp $(COMMON) $(CODE) $(LIB) $(LIBPMEMKV)
	$(MAKE) -j -C rocksdb/vanilla static_lib
	CXXFLAGS=-I$(realpath ../src) $(MAKE) -j -C rocksdb/pronto static_lib
	$(CXX) $(CXXFLAGS) main.cpp $(COMMON) $(LIB) -o $@ $(LDFLAGS)

sort-vector: sort-vector.cpp $(LIB)
	$(CXX) -std=c++11 -fno-stack-protector -o3 sort-vector.cpp $(LIB) -o $@ \
		-pthread -lpmem -luuid -lstdc++fs

clean:
	$(RM) -f $(TARGET)
	$(RM) -f sort-vector
