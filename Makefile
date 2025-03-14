# Values

SHELL := /bin/bash

LIB_NAME = pure
LIB = lib$(LIB_NAME).so

CC = g++-14
CPPFLAGS = -c -Wall -Wextra -Werror
SOFLAGS = -shared -o
LFLAGS = -L. -l$(LIB_NAME)

SRCS =  router

SRCS_CPP = $(addprefix src/, $(addsuffix .cpp, $(SRCS)))
SRCS_O = $(SRCS_CPP:.cpp=.o)

TEST = test.exe
TEST_CPP = $(addprefix test/, $(addsuffix _test.cpp, $(SRCS) main core))
TEST_O = $(TEST_CPP:.cpp=.o)

# Templates

define CHECK_VAR_TMP
if [[ -z "$$module" && -z "$$submodule" ]]; then \
	echo "Usage: make $(1) module=<module or submodule name>";\
	exit 1;\
fi;
endef

# Commands

all: $(LIB)

$(LIB): $(SRCS_O)
	$(CC) $(SOFLAGS) $@ $^

src/%.o: src/%.cpp
	$(CC) -fPIC $(CPPFLAGS) -Iinclude $^ -o $@

test: $(LIB) $(TEST)
	./$(TEST)

$(TEST): $(TEST_O)
	$(CC) $^ -o $@

test/%.o: test/%.cpp
	$(CC) $(CPPFLAGS) $^ -o $@ -Itest/include

clean:
	@rm -rf $(SRCS_O) $(TEST_O)
	@echo "Objects cleaned!"

fclean: clean
	@rm -rf $(LIB) $(TEST)
	@echo "Archive cleaned"

re: fclean all

re_test: fclean test

generate:
	@$(call CHECK_VAR_TMP,generate) \
	FILES=`find . -name "$$module*.cpp" -o -name "$$module*.hpp" -o -name "$$module*.o"`;\
	if [ -n "$$FILES" ] && [ -n "$$module" ]; then \
		echo "module $$module already exists.";\
		exit 1;\
	fi;\
	FILES=`find . -name "$$submodule*.cpp" -o -name "$$submodule*.hpp" -o -name "$$submodule*.o"`;\
	if [ -n "$$FILES" ] && [ -n "$$submodule" ]; then \
		echo "submodule $$submodule already exists.";\
		exit 1;\
	fi;\
	if [ -n "$$module" ]; then \
		UPPER=$$(echo $$module | tr '[:lower:]' '[:upper:]'); \
		mkdir -p src include test/include;\
		echo '/*--- Code file for '$$module' ---*/' > src/$$module.cpp;\
		echo '' >> src/$$module.cpp;\
		echo '#include "../include/'"$$module"'.hpp"' >> src/$$module.cpp; \
		echo '/*--- Header file for '$$module' ---*/' > include/$$module.hpp;\
		echo '' >> include/$$module.hpp;\
		echo '#ifndef '"$$UPPER"'_HPP' >> include/$$module.hpp; \
		echo '#define '"$$UPPER"'_HPP' >> include/$$module.hpp; \
		echo '' >> include/$$module.hpp; \
		echo '#endif' >> include/$$module.hpp; \
		echo '/*--- Code file for test '$$module' ---*/' > test/$${module}_test.cpp;\
		echo '' >> test/$${module}_test.cpp;\
		echo '#include "./include/'"$$module"'_test.hpp"' >> test/$${module}_test.cpp; \
		echo '/*--- Header file for test '$$module' ---*/' > test/include/$${module}_test.hpp;\
		echo '' >> test/include/$${module}_test.hpp;\
		echo '#ifndef '"$$UPPER"'_TEST_HPP' >> test/include/$${module}_test.hpp; \
		echo '#define '"$$UPPER"'_TEST_HPP' >> test/include/$${module}_test.hpp; \
		echo '' >> test/include/$${module}_test.hpp; \
		echo '#include "../../include/'"$$module"'.hpp"' >> test/include/$${module}_test.hpp; \
		echo '' >> test/include/$${module}_test.hpp; \
		echo '#endif' >> test/include/$${module}_test.hpp; \
		echo "Created src/$$module.cpp, include/$$module.hpp, test/$${module}_test.cpp and test/include/$${module}_test.hpp"; \
		sed -i -e "/^SRCS[[:space:]]*=/ s/$$/ $$module/" Makefile; \
		echo "Updated SRCS in Makefile";\
		sed -i "1i #include \"./include/${module}_test.hpp\"" ./test/main_test.cpp; \
		echo "Updated #include in test/main_test.cpp";\
	else\
		echo "submodule routine is not implemented yet";\
	#	mkdir -p src/$$submodule/include test/include;\
	#	UPPER=$$(echo $$submodule | tr '[:lower:]' '[:upper:]'); \
	#	echo '#include "./include/'"$$submodule"'.hpp"' > src/$$submodule/$$submodule.cpp;\
	#	echo '#ifndef '"$$UPPER"'_HPP' > src/$$submodule/include/$$submodule.hpp;\
	#	echo '#define '"$$UPPER"'_HPP' >> src/$$submodule/include/$$submodule.hpp;\
	#	echo '' >> src/$$submodule/include/$$submodule.hpp;\
	#	echo '#endif' >> src/$$submodule/include/$$submodule.hpp;\
	#	echo '#include "./include/'"$$module"'.hpp"' > test/$${module}_test.cpp; \
	#	echo '#ifndef '"$$UPPER"'_TEST_HPP' > test/include/$${module}_test.hpp; \
	#	echo '#define '"$$UPPER"'_TEST_HPP' >> test/include/$${module}_test.hpp; \
	#	echo '' >> test/include/$${module}_test.hpp; \
	#	echo '#include "../../include/'"$$module"'.hpp"' >> test/include/$${module}_test.hpp; \
	#	echo '#endif' >> test/include/$${module}_test.hpp; \
	fi;

delete:
	@$(call CHECK_VAR_TMP,delete) \
	FILES=`find . -name "$$module*.cpp" -o -name "$$module*.hpp" -o -name "$$module*.o"`;\
	if [[ -n "$$FILES" && -n "$$module" ]]; then \
		echo "Found $$FILES";\
		read -p "Are you sure you want to erase these modules? (y/n): " ASW;\
		if [ "$$ASW" = "n" ]; then \
			echo "Aborting";\
			exit 0;\
		fi;\
		echo "$$FILES" | xargs rm -f; \
		sed -i "s/\b$$module\b//g" Makefile; \
		sed -i "/#include \"\.\/include\/${module}_test.hpp\"/d" test/main_test.cpp; \
		echo "Removed files for $$module and updated Makefile and test/main_test.cpp";\
	elif [ -n "$$submodule" ]; then \
		echo "submodule routine not implemented yet";\
		exit 0;\
	else\
		echo "$$module files not found";\
		exit 1;\
	fi;

clear: # To be used when you want to clear before you make re.
	clear

.PHONY: all test clean fclean re re_test generate delete clear