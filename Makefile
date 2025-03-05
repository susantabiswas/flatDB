TARGET = db
DB_FILENAME = testdb.db
ARGS = $(DB_FILENAME)

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
	$(RUN_PREFIX)$(TARGET) $(ARGS)

debug: $(TARGET)
	$(RUN_PREFIX)$(TARGET) $(ARGS) --debug

# Usage: make test
test: $(TARGET) 
	@echo "Running tests"
	bundle exec rspec

# Usage: make clean
clean: $(TARGET)
	@echo "Cleaning build files"
	$(RM) $(TARGET) $(DB_FILENAME)

clear:
	@echo "Cleaning database file: $(file)"

	$(RM) $(if $(file), $(file), $(DB_FILENAME))
	
# For commands that don't create files and are to run always
.PHONY: run test clean clear