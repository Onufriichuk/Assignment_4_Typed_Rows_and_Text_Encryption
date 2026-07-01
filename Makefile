CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra

UNAME_S := $(shell uname -s)

ifeq ($(UNAME_S),Darwin)
	LIB_NAME = libcipher.dylib
	LIB_FLAGS = -dynamiclib
	DL_FLAGS = -ldl
else
	LIB_NAME = libcipher.so
	LIB_FLAGS = -shared -fPIC
	DL_FLAGS = -ldl
endif

all: $(LIB_NAME) editor

$(LIB_NAME): cipher.cpp cipher_api.h
	$(CXX) $(CXXFLAGS) $(LIB_FLAGS) cipher.cpp -o $(LIB_NAME)

editor: main.cpp
	$(CXX) $(CXXFLAGS) main.cpp -o editor $(DL_FLAGS)

run: all
	./editor

clean:
	rm -f editor libcipher.so libcipher.dylib cipher.dll *.txt *.bin