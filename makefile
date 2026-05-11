CXX=g++
CXXFLAGS = -std=c++17 -I. -pthread
MAIN_BIN = ./bin/main
TEST_BIN = ./bin/tests
STRESS_BIN = ./bin/stress

skiplist: main.o
	$(CXX) -o $(MAIN_BIN) main.o $(CXXFLAGS)
	rm -f ./*.o

test: tests/test_main.cpp tests/test_utils.h skiplist.h
	$(CXX) tests/test_main.cpp -o $(TEST_BIN) $(CXXFLAGS)
	$(TEST_BIN)
	rm -f ./*.o

stress: stress-test/stress_test.cpp skiplist.h
	$(CXX) stress-test/stress_test.cpp -o $(STRESS_BIN) $(CXXFLAGS)

clean:
	rm -f ./*.o $(TEST_BIN) $(STRESS_BIN)
