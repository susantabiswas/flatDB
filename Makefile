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


$(TARGET): db.cpp
	@echo "Building project"
	g++ db.cpp -o $(TARGET)

run: $(TARGET)
	$(RUN_PREFIX).$(TARGET)

test: $(TARGET)
	@echo "Running tests"
	bundle exec rspec

clean: $(TARGET)
	@echo "Cleaning build files"
	$(RM) $(TARGET)

.PHONY: run test clean