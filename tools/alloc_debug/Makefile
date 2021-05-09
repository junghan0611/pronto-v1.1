CXX=g++
#CXXFLAGS=-std=c++14 -ggdb
CXXFLAGS=-std=c++14 -o3 -fno-stack-protector
LDFLAGS=-luuid

.phony: ckpt_alloc_test

ifdef ALLOC_SIZE
CXXFLAGS+=-DALLOC_SIZE=$(ALLOC_SIZE)
endif

ckpt_alloc_test: ckpt_alloc_test.cpp ../../src/ckpt_alloc.cpp ../../src/cpu_info.cpp ../../src/ckpt_alloc.hpp
	$(CXX) $(CXXFLAGS) -o $@ $(word 1,$^) $(word 2,$^) $(word 3,$^) $(LDFLAGS)

clean:
	$(RM) -f ckpt_alloc_test
