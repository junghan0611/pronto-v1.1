CXX=g++
CXXFLAGS=-std=c++11 -ggdb -fno-stack-protector
LDFLAGS=-lpmem -luuid

all: dump_log dump_snapshot

dump_log: dump_log.cpp nv_log.o
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

nv_log.o: ../src/nv_log.cpp ../src/nv_log.hpp ../src/savitar.hpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

dump_snapshot: dump_snapshot.cpp ../src/ckpt_alloc.cpp ../src/cpu_info.cpp
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	$(RM) -f dump_log
	$(RM) -f dump_snapshot
	$(RM) -f *.o
