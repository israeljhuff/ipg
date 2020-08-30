// ----------------------------------------------------------------------------
// example main function for quickly testing a new parser
//
// to build and run on Linux or Windows (Cygwin):
//  g++ --std=c++11 example_main.cpp -o example_parser.exe
//  ./exampler_parser.exe SOMEFILENAME
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
	if (nullptr == fp)
	{
		eprintln("ERROR opening file: ", argv[1]);
		return 1;
	}
	fseek(fp, 0, SEEK_END);
	size_t file_len = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	char *buf = new char[file_len + 1];
	buf[file_len] = '\0';
	size_t bytes_read = fread(buf, 1, file_len, fp);
	fclose(fp);
	ASTNode astn(0, 1, 1, "ROOT");
	Parser p(buf);
	if (RET_OK != p.parse(astn))
	{
		eprintln("ERROR parsing");
		eprintln("last fully-parsed element is before line ", p.line(),
			", col ", p.col(), ", file position ", p.pos(), " of ", p.len());
		eprintln("last partially-parsed element is before line ",
			p.line_ok(), ", col ", p.col_ok());
	}
	else
	{
		astn.print();
		eprintln("parsed successfully");

		Evaluator e;
		// skip "ROOT" node and assume 1 child node
		// NOTE: this must be changed if multiple top-level nodes allowed
		if (e.eval(astn.child(0))) eprintln("evaluated successfully");
		else eprintln("ERROR evaluating");
	}
	delete[] buf;
	return 0;
}
