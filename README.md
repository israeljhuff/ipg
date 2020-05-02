Israel's Parser Generator (IPG)
Author: Israel Huff
https://github.com/israeljhuff/ipg

Compile and boostrap from grammar definition:
Linux
g++ --std=c++11 ipg.cpp && ./a.out ipg.grammar > tmp.cpp
g++ --std=c++11 tmp.cpp && ./a.out ipg.grammar

Windows (Cygwin)
g++ --std=c++11 ipg.cpp && ./a ipg.grammar > tmp.cpp
g++ --std=c++11 tmp.cpp && ./a ipg.grammar
