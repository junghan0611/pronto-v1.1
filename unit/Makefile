CXX=g++
CXXFLAGS=-std=c++14 -fno-stack-protector
LDFLAGS=-luuid -lgtest -lgtest_main -lpthread -lstdc++fs -lpmem
TARGET=test
DEPS=ckpt_alloc.o cpu_info.o snapshot.o nvm_manager.o nv_object.o nv_catalog.o nv_factory.o thread.o nv_log.o persister.o

all: $(TARGET)

%.o: ../src/%.cpp ../src/%.hpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

cpu_info.o: ../src/cpu_info.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(TARGET): main.cpp *.hpp $(DEPS)
	$(CXX) $(CXXFLAGS) -o $@ $< $(DEPS) $(LDFLAGS)

clean:
	$(RM) -f *.o
	$(RM) -f $(TARGET)
