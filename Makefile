# Release cmake configuration
build_release/Makefile:
	@mkdir -p build_release
	@cd build_release && cmake -DCMAKE_BUILD_TYPE=Release ..

# Debug cmake configuration
build_debug/Makefile:
	@mkdir -p build_debug
	@cd build_debug && cmake -DCMAKE_BUILD_TYPE=Debug -DASAN_ENABLED=True ..

# Run cmake configuration
.PHONY: cmake-debug cmake-release
cmake-debug cmake-release: cmake-%: build_%/Makefile

# Build using cmake
.PHONY: build-debug build-release
build-debug build-release: build-%: cmake-%
	@cmake --build build_$* -j $(shell nproc)
	@make clean-data

# Run
.PHONY: run-debug run-release
run-debug run-release: run-%: build-%
	@./build_$*/cache -s 1

# Run with `clean` step
.PHONY: clean-run-debug clean-run-release
clean-run-debug clean-run-release: clean-run-%: clean
	@make run-$*

# Cleanup data
.PHONY: clean
clean:
	@rm -rf build_*

.PHONY: clean-data
clean-data:
	@rm -rf ./data/*

# Format the sources
.PHONY: format
format:
	@find core -name '*pp' -type f | xargs clang-format -i
	@find test -name '*pp' -type f | xargs clang-format -i
	@find benchmark -name '*pp' -type f | xargs clang-format -i
	@clang-format -i main.cpp

# Run tests in debug
.PHONY: tests
tests: build-debug
	@cd build_debug && ./test/cache_test -V
