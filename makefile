OUTPUT_DIR = bin
EXEC = $(OUTPUT_DIR)/test_generator

create_input: build_generator
	./$(EXEC)

build_generator: test_generator.c | $(OUTPUT_DIR)
	gcc -o $(EXEC) test_generator.c

$(OUTPUT_DIR):
	mkdir -p $(OUTPUT_DIR)

clean:
	rm -rf $(OUTPUT_DIR) processes.txt

.PHONY: create_input build_generator clean