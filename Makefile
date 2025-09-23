CXX = g++
CXXFLAGS = -std=c++11 -Wall -Wextra -pedantic
TARGET = client
SOURCE = clientmain.cpp

all: $(TARGET)

$(TARGET): $(SOURCE)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SOURCE)

clean:
	rm -f $(TARGET) *.o

.PHONY: all clean
