// ----------------------------------------------------------------------------
// example main function for quickly testing a new parser
//
// to build on Linux or Windows (Cygwin):
//  g++ --std=c++11 example_main.cpp -o example_parser.exe
//
//  NOTE: assumes parser saved to "example_parser.h"

#include "example_parser.h"

using namespace IPG;

int main(int argc, char **argv)
{
	if (argc < 2)
	{
		eprintln("Usage: ", argv[0], " <filename>");
		return 1;
	}
	FILE *fp;
	fp = fopen(argv[1], "rb");
	fseek(fp, 0, SEEK_END);
	size_t file_len = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	char *buf = new char[file_len + 1];
	buf[file_len] = '\0';
	size_t bytes_read = fread(buf, 1, file_len, fp);
	fclose(fp);
	ASTNode astn(0, "ROOT");
	Parser p(buf);
	int32_t retval = p.parse(astn);
	if (RET_FAIL == retval || p.pos() < p.len())
	{
		eprintln("ERROR parsing");
		eprintln("last fully-parsed element is before line ",
			p.line(), ", col ", p.col(), ", file position ", p.pos(), " of ", p.len());
		eprintln("last partially-parsed element is before line %d, col %d",
			p.line_ok(), p.col_ok());
	}
	else
	{
		astn.print();
		eprintln("parsed successfully");
	}
	delete[] buf;
	return 0;
}
