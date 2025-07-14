CXX = g++
CXXFLAGS = -Wall -Wpedantic -Werror -std=c++20

SRC = main.cpp http_request.cpp http_response.cpp
OBJ = $(SRC:.cpp=.o)
TARGET = server

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET)
