CXX = g++
CXXFLAGS = -O3 -Wall -std=c++17

SRCS = src/main.cpp src/graph.cpp src/validator.cpp src/mst.cpp src/optimizer.cpp
OBJS = $(SRCS:.cpp=.o)
TARGET = steiner_solver

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)