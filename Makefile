# Values

SHELL := /bin/bash

BUILD_DIR = build
CMAKE_FLAGS = -DCMAKE_CXX_COMPILER=g++-14 \
              -DCMAKE_CXX_STANDARD=23 \
              -DCMAKE_CXX_STANDARD_REQUIRED=OFF \
              -DCMAKE_CXX_EXTENSIONS=OFF \
              -DCMAKE_UNITY_BUILD=ON
# Detect number of CPU cores for parallel compilation
NPROC = $(shell nproc)

# Commands

all: $(BUILD_DIR)
	@cd $(BUILD_DIR) && cmake --build . -j$(NPROC)
	@cd $(BUILD_DIR) && ctest --output-on-failure

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)
	@cd $(BUILD_DIR) && cmake $(CMAKE_FLAGS) -G Ninja .. 

clean:
	@rm -rf $(BUILD_DIR)
	@echo "Build directory cleaned!"

re: clean all

# Module generation and deletion utilities
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
		mkdir -p include/feather test/include;\
		echo '/*--- Header file for '$$module' ---*/' > include/feather/$$module.hpp;\
		echo '' >> include/feather/$$module.hpp;\
		echo '#ifndef FEATHER_'"$$UPPER"'_HPP' >> include/feather/$$module.hpp; \
		echo '#define FEATHER_'"$$UPPER"'_HPP' >> include/feather/$$module.hpp; \
		echo '' >> include/feather/$$module.hpp; \
		echo '#endif' >> include/feather/$$module.hpp; \
		echo '/*--- Code file for test '$$module' ---*/' > test/$${module}_test.cpp;\
		echo '' >> test/$${module}_test.cpp;\
		echo '#include <'"$$module"'_test.hpp>' >> test/$${module}_test.cpp; \
		echo '/*--- Header file for test '$$module' ---*/' > test/include/$${module}_test.hpp;\
		echo '' >> test/include/$${module}_test.hpp;\
		echo '#ifndef FEATHER_'"$$UPPER"'_TEST_HPP' >> test/include/$${module}_test.hpp; \
		echo '#define FEATHER_'"$$UPPER"'_TEST_HPP' >> test/include/$${module}_test.hpp; \
		echo '' >> test/include/$${module}_test.hpp; \
		echo '#include <feather/'"$$module"'.hpp>' >> test/include/$${module}_test.hpp; \
		echo '' >> test/include/$${module}_test.hpp; \
		echo '#endif' >> test/include/$${module}_test.hpp; \
		echo "Created include/feather/$$module.hpp, test/$${module}_test.cpp and test/include/$${module}_test.hpp"; \
		sed -i "9i #include <feather/${module}.hpp>" ./include/feather.hpp; \
		echo "Updated #include in include/feather.hpp";\
		sed -i "s/set(TEST_TARGETS/set(TEST_TARGETS\n    ${module}_test/" ./test/CMakeLists.txt;\
		echo "Updated test/CMakeLists.txt with new test target!";\
		echo "Don't forget to update CMakeLists.txt if you need any additional configuration!";\
	else\
		echo "submodule routine is not implemented yet";\
	fi;

delete:
	@$(call CHECK_VAR_TMP,delete) \
	if [[ -n "$$module" ]]; then \
		FILES="include/feather/$$module.hpp test/$${module}_test.cpp test/include/$${module}_test.hpp";\
		if [ -f "include/feather/$$module.hpp" ]; then \
			echo "Found $$FILES";\
			read -p "Are you sure you want to erase these modules? (y/n): " ASW;\
			if [ "$$ASW" = "n" ]; then \
				echo "Aborting";\
				exit 0;\
			fi;\
			rm -f $$FILES; \
			echo "Removed files for $$module";\
			sed -i "/    ${module}_test/d" ./test/CMakeLists.txt;\
			echo "Updated test/CMakeLists.txt to remove test target!";\
			sed -i "/#include <feather\/${module}.hpp>/d" ./include/feather.hpp;\
			echo "Removed include from feather.hpp";\
		else\
			echo "$$module files not found";\
			exit 1;\
		fi;\
	elif [ -n "$$submodule" ]; then \
		echo "submodule routine not implemented yet";\
		exit 0;\
	fi;

clear:
	clear

define CHECK_VAR_TMP
if [[ -z "$$module" && -z "$$submodule" ]]; then \
	echo "Usage: make $(1) module=<module or submodule name>";\
	exit 1;\
fi;
endef

.PHONY: all clean re generate delete clear