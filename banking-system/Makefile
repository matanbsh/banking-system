# Compiler and Flags
CXX = g++
CXXFLAGS = -std=c++11 -Wall -Werror -pedantic-errors -DNDEBUG -g -pthread

# Target Executable
TARGET = bank

# Source and Object Files
SRCS = main.cpp banking_system.cpp read_write_lock.cpp task_queue.cpp thread_pool.cpp
OBJS = $(SRCS:.cpp=.o)

# Default Rule: Build the Program
all: $(TARGET)

# Link the Executable
$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

# Compile C++ Files
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@ 

# Clean Rule: Remove Compilation Products
clean:
	rm -f $(OBJS) $(TARGET)

# Phony Targets
.PHONY: all clean