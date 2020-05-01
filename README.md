# ipg
Windows
g++ --std=c++11 ipg.cpp && ./a ipg.grammar > tmp.cpp
g++ --std=c++11 tmp.cpp && ./a ipg.grammar
