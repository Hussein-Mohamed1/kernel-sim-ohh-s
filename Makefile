TARGET_EXEC := os-sim
KERNEL_EXEC := os-sim
PROCESS_EXEC := process

BUILD_DIR := ./build
KERNEL_DIR := ./src/kernel
SHARED_MEM_DIR := ./src/shared_mem
PROCESS_DIR := ./src/process
DATA_STRUCTURES_DIR := ./src/data_structures

# Add shared_mem source
SHARED_MEM_SRC := $(SHARED_MEM_DIR)/shared_mem.c
SHARED_MEM_OBJ := $(BUILD_DIR)/$(SHARED_MEM_DIR)/shared_mem.c.o

# Find source files for each component
KERNEL_ONLY_SRCS := $(shell find $(KERNEL_DIR) -name '*.cpp' -or -name '*.c' -not -name 'clk.c' -or -name '*.s')
PROCESS_SRCS := $(shell find $(PROCESS_DIR) -name '*.cpp' -or -name '*.c' -or -name '*.s')
DATA_STRUCTURES_SRCS := $(shell find $(DATA_STRUCTURES_DIR) -name '*.cpp' -or -name '*.c' -or -name '*.s')
CLK_SRCS := $(KERNEL_DIR)/clk.c

# Add shared_mem to both kernel and process objects
KERNEL_ONLY_OBJS := $(KERNEL_ONLY_SRCS:%=$(BUILD_DIR)/%.o) $(SHARED_MEM_OBJ)
PROCESS_OBJS := $(PROCESS_SRCS:%=$(BUILD_DIR)/%.o) $(SHARED_MEM_OBJ)
DATA_STRUCTURES_OBJS := $(DATA_STRUCTURES_SRCS:%=$(BUILD_DIR)/%.o)
CLK_OBJS := $(CLK_SRCS:%=$(BUILD_DIR)/%.o)

# All dependencies
DEPS := $(KERNEL_ONLY_OBJS:.o=.d) $(PROCESS_OBJS:.o=.d) $(DATA_STRUCTURES_OBJS:.o=.d) $(CLK_OBJS:.o=.d)

# Include directories
INC_DIRS := $(shell find ./src -type d)
INC_FLAGS := $(addprefix -I,$(INC_DIRS)) -g

# Compiler flags
CPPFLAGS := $(INC_FLAGS) -MMD -MP
#LDFLAGS := -lreadline

# Default target builds everything
all: kernel process

# Kernel executable
kernel: $(KERNEL_ONLY_OBJS) $(CLK_OBJS) $(DATA_STRUCTURES_OBJS)
	@echo "Building kernel..."
	mkdir -p $(BUILD_DIR)
	$(CXX) $(KERNEL_ONLY_OBJS) $(CLK_OBJS) $(DATA_STRUCTURES_OBJS) -o $(KERNEL_EXEC) $(LDFLAGS)

# Process executable
process: $(PROCESS_OBJS) $(CLK_OBJS) $(DATA_STRUCTURES_OBJS)
	@echo "Building process component..."
	mkdir -p $(BUILD_DIR)
	$(CC) $(PROCESS_OBJS) $(CLK_OBJS) $(DATA_STRUCTURES_OBJS) -o $(PROCESS_EXEC) $(LDFLAGS)

# Build step for C source
$(BUILD_DIR)/%.c.o: %.c
	mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

# Build step for C++ source
$(BUILD_DIR)/%.cpp.o: %.cpp
	mkdir -p $(dir $@)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

# Build step for assembly
$(BUILD_DIR)/%.s.o: %.s
	mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) -c $< -o $@

.PHONY: all kernel process clean
clean:
	rm -rf $(BUILD_DIR)
	rm -f ./$(KERNEL_EXEC) ./$(PROCESS_EXEC)

-include $(DEPS)