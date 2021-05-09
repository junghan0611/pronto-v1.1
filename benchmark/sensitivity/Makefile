CXX:=g++
CXXFLAGS:=-std=c++11 -o3 -fno-stack-protector
LDFLAGS:=-I../../src -pthread -lpmem -luuid -lstdc++fs
LIB:=../../src/libsavitar.a
TARGET:=benchmark

all: $(TARGET)

$(TARGET): benchmark.cpp
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LIB) $(LDFLAGS)

clean:
	$(RM) -f $(TARGET)
