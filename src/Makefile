AR=ar
CXX=g++
CXXFLAGS=-std=c++17
CXXFLAGS+=-ggdb
CXXFLAGS+=-fno-stack-protector
CXXFLAGS+=-o3
LDFLAGS=-I./ -pthread -lpmem -luuid
TARGET=libsavitar.a

all: $(TARGET)

ifdef DEBUG
CXXFLAGS+=-DDEBUG
endif

ifdef LOG_SIZE
CXXFLAGS+=-DLOG_SIZE="((off_t)$(LOG_SIZE) << 30)"
endif

ifdef DISABLE_HT_PINNING
CXXFLAGS+=-DNO_HT_PINNING
endif

ifdef PRONTO_SYNC
CXXFLAGS+=-DSYNC_SL # no ASL
endif

$(TARGET): thread.o persister.o nv_log.o nv_object.o context.o cpu_info.o nv_catalog.o nvm_manager.o nv_factory.o ckpt_alloc.o snapshot.o
	$(AR) rvs $@ $^

ckpt_alloc.o: ckpt_alloc.cpp ckpt_alloc.hpp
	$(CXX) -c $(CXXFLAGS) -o $@ $<

snapshot.o: snapshot.cpp snapshot.hpp
	$(CXX) -c $(CXXFLAGS) -o $@ $<

thread.o: thread.cpp thread.hpp
	$(CXX) -c $(CXXFLAGS) -o $@ $<

persister.o: persister.cpp persister.hpp
	$(CXX) -c $(CXXFLAGS) -o $@ $<

nv_log.o: nv_log.cpp nv_log.hpp
	$(CXX) -c $(CXXFLAGS) -o $@ $<

nv_object.o: nv_object.cpp nv_object.hpp recovery_context.hpp
	$(CXX) -c $(CXXFLAGS) -o $@ $<

context.o: context.cpp
	$(CXX) -c $(CXXFLAGS) -o $@ $^

cpu_info.o: cpu_info.cpp
	$(CXX) -c $(CXXFLAGS) -o $@ $^

nv_catalog.o: nv_catalog.cpp nv_catalog.hpp
	$(CXX) -c $(CXXFLAGS) -o $@ $<

nvm_manager.o: nvm_manager.cpp nvm_manager.hpp recovery_context.hpp
	$(CXX) -c $(CXXFLAGS) -o $@ $<

nv_factory.o: nv_factory.cpp nv_factory.hpp
	$(CXX) -c $(CXXFLAGS) -o $@ $<

clean:
	rm -f *.o
	rm -f $(TARGET)
