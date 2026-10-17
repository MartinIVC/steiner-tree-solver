CXX = g++
CXXFLAGS = -O3 -Wall -Wextra -Wpedantic -std=c++17 -MMD -MP

BUILD_DIR = build
LIB_SRCS = src/graph.cpp src/validator.cpp src/mst.cpp src/optimizer.cpp
LIB_OBJS = $(patsubst src/%.cpp,$(BUILD_DIR)/%.o,$(LIB_SRCS))

OBJS = $(BUILD_DIR)/main.o $(LIB_OBJS)
TEST_OBJS = $(BUILD_DIR)/tests/test_solver.o $(LIB_OBJS)
DEPS = $(OBJS:.o=.d) $(BUILD_DIR)/tests/test_solver.d

TARGET = steiner_solver
TEST_TARGET = $(BUILD_DIR)/test_solver

.PHONY: all clean test plot

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJS)

$(BUILD_DIR)/%.o: src/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/tests/%.o: tests/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -Isrc -c $< -o $@

test: $(TEST_TARGET)
	./$(TEST_TARGET)

$(TEST_TARGET): $(TEST_OBJS)
	$(CXX) $(CXXFLAGS) -o $(TEST_TARGET) $(TEST_OBJS)

plot:
	python3 scripts/plot_tree.py

clean:
	rm -rf $(BUILD_DIR) src/*.o $(TARGET)

-include $(DEPS)