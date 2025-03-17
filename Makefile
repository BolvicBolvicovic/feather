# Values

SHELL := /bin/bash

CC = g++-14
CPPFLAGS = -c -Wall -Wextra -Werror
LFLAGS = -L. -l$(LIB_NAME)

SRCS =  router

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

all: $(TEST)
	./$(TEST)

$(TEST): $(TEST_O)
	$(CC) $^ -o $@

test/%.o: test/%.cpp
	$(CC) $(CPPFLAGS) $^ -o $@ -Itest/include -Iinclude

clean:
	@rm -rf $(TEST_O)
	@echo "Test objects cleaned!"

fclean: clean
	@rm -rf $(TEST)
	@echo "Test executable cleaned"

re: fclean all

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
		echo '/*--- Header file for '$$module' ---*/' > include/feather/$$module.hpp;\
		echo '' >> include/feather/$$module.hpp;\
		echo '#ifndef '"$$UPPER"'_HPP' >> include/feather/$$module.hpp; \
		echo '#define '"$$UPPER"'_HPP' >> include/feather/$$module.hpp; \
		echo '' >> include/feather/$$module.hpp; \
		echo '#endif' >> include/feather/$$module.hpp; \
		echo '/*--- Code file for test '$$module' ---*/' > test/$${module}_test.cpp;\
		echo '' >> test/$${module}_test.cpp;\
		echo '#include <'"$$module"'_test.hpp>' >> test/$${module}_test.cpp; \
		echo '/*--- Header file for test '$$module' ---*/' > test/include/$${module}_test.hpp;\
		echo '' >> test/include/$${module}_test.hpp;\
		echo '#ifndef '"$$UPPER"'_TEST_HPP' >> test/include/$${module}_test.hpp; \
		echo '#define '"$$UPPER"'_TEST_HPP' >> test/include/$${module}_test.hpp; \
		echo '' >> test/include/$${module}_test.hpp; \
		echo '#include <feather/'"$$module"'.hpp>' >> test/include/$${module}_test.hpp; \
		echo '' >> test/include/$${module}_test.hpp; \
		echo '#endif' >> test/include/$${module}_test.hpp; \
		echo "Created src/$$module.cpp, include/$$module.hpp, test/$${module}_test.cpp and test/include/$${module}_test.hpp"; \
		sed -i -e "/^SRCS[[:space:]]*=/ s/$$/ $$module/" Makefile; \
		echo "Updated SRCS in Makefile";\
		sed -i "1i #include <${module}_test.hpp>" ./test/main_test.cpp; \
		sed -i "9i #include <feather/${module}.hpp>" ./include/feather.hpp; \
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

.PHONY: all test clean fclean re generate delete clear