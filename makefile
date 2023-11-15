make:
	c++ -std=c++17 -o program_name CommandLineTest.cpp -L ./MemoryManager -lMemoryManager
	valgrind ./program_name