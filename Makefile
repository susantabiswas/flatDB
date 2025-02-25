TARGET = db

# OS specific part
ifeq ($(OS), Windows_NT)
	TARGET := $(TARGET).exe
	RM = del
	RUN_PREFIX =
else
	RM = rm -f
	RUN_PREFIX = ./
endif

# Usage: make
$(TARGET): db.cpp
	@echo "Building project"
	g++ db.cpp -o $(TARGET)

# Usage: make run
run: $(TARGET)
	$(RUN_PREFIX)$(TARGET)

debug: $(TARGET)
	$(RUN_PREFIX)$(TARGET) --debug

# Usage: make test
test: $(TARGET)
	@echo "Running tests"
	bundle exec rspec

# Usage: make clean
clean: $(TARGET)
	@echo "Cleaning build files"
	$(RM) $(TARGET)

# For commands that don't create files and are to run always
.PHONY: run test clean