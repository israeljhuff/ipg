Israel's Parser Generator (IPG)
Author: Israel Huff
https://github.com/israeljhuff/ipg

Compile and boostrap from grammar definition on Linux or Windows (Cygwin):
g++ --std=c++11 ipg.cpp -o ipg.exe
./ipg.exe ipg.grammar > example_parser.h
g++ --std=c++11 example_main.cpp -o example_parser.exe
./example_parser.exe ipg.grammar
