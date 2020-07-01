// Israel's Parser Generator (IPG)
// Author: Israel Huff
// https://github.com/israeljhuff/ipg

// Linux
// g++ --std=c++11 ipg.cpp && ./a.out ipg.grammar > tmp.cpp
// g++ --std=c++11 tmp.cpp && ./a.out tmp.grammar

// Windows (Cygwin)
// g++ --std=c++11 ipg.cpp -o ipg.exe && ./ipg.exe ipg.grammar > tmp.cpp
// g++ --std=c++11 tmp.cpp && ./a.exe tmp.grammar

// TODO: store errors, then only print last one since previous ones may just be due to alternate parsing
// TODO: when parsing a rule (or alts?), only clear errors that occurred before it
// TODO: track position of last failed match in instance variable (eg. if failed to find group closing paren, report that line/position)
//       and report correct positions of nested errors
// TODO: vector instead of map for rule list?

#include <cstdint>
#include <cstdio>
#include <map>
#include <vector>

#define SCC_DEBUG 0

// ----------------------------------------------------------------------------
namespace IPG
{
// ----------------------------------------------------------------------------
// types of elements
enum class ElemType : uint32_t
{
	NAME, ALT, GROUP, STRING, CH_CLASS
};

// ----------------------------------------------------------------------------
// types of quantifiers
enum class QuantifierType : uint32_t
{
	ONE, ZERO_ONE, ZERO_PLUS, ONE_PLUS
};

// ----------------------------------------------------------------------------
// abstract syntax tree node
class ASTNode
{
public:
	ASTNode() {}
	ASTNode(uint32_t pos, std::string text) { m_pos = pos; m_text = text; }
	void clear() { m_pos = 0; m_text.clear(); m_children.clear(); }
	uint32_t pos() { return m_pos; }
	std::string text() { return m_text; }
	void add_child(ASTNode &child) { m_children.push_back(child); }
    ASTNode &child(uint32_t index) { return m_children[index]; }
	std::vector<ASTNode> &children() { return m_children; }
	void print(uint32_t depth = 0)
	{
		printf("%s%s", std::string(depth * 2, ' ').c_str(), m_text.c_str());
		if (m_children.size() > 0) printf(": %lu", m_children.size());
		printf("\n");
		for (auto child : m_children) child.print(depth + 1);
	}
private:
	uint32_t m_pos = 0;
	std::string m_text;
	std::vector<ASTNode> m_children;
};

// ----------------------------------------------------------------------------
// element
class Elem
{
public:
	Elem(ElemType type)
	{
		m_type = type;
		m_quantifier = QuantifierType::ONE;
	}

	Elem(ElemType type, std::string text)
	{
		m_type = type;
		m_quantifier = QuantifierType::ONE;
		m_text.push_back(text);
	}

	std::map<ElemType, std::string> m_elem_strs =
	{
		{ ElemType::NAME, "name" },
		{ ElemType::ALT, "alt" },
		{ ElemType::GROUP, "group" },
		{ ElemType::STRING, "string" },
		{ ElemType::CH_CLASS, "character class" }
	};

	std::map<QuantifierType, std::string> m_quantifier_strs =
	{
		{ QuantifierType::ONE, "" },
		{ QuantifierType::ZERO_ONE, "?" },
		{ QuantifierType::ZERO_PLUS, "*" },
		{ QuantifierType::ONE_PLUS, "+" }
	};

	ElemType type() { return m_type; }
	std::vector<std::string> &text() { return m_text; }
	QuantifierType &quantifier() { return m_quantifier; }
	std::vector<Elem> &sub_elems() { return m_sub_elems; }
	std::map<QuantifierType, std::string> &quantifier_strs()
	{ return m_quantifier_strs; }

	std::string to_string()
	{
		std::string str;
		if (m_sub_elems.size() > 0)
		{
			if (ElemType::ALT == m_type) str += " |";
			else if (ElemType::GROUP == m_type) str += " (";
			for (auto sub_elem : m_sub_elems)
			{
				str += sub_elem.to_string();
			}
			if (ElemType::GROUP == m_type) str += " )";
		}
		else
		{
			for (auto item : m_text) str += " " + item;
		}
		str += m_quantifier_strs[m_quantifier];
		return str;
	}

private:
	ElemType m_type;
	std::vector<std::string> m_text;
	QuantifierType m_quantifier;
	std::vector<Elem> m_sub_elems;
};

// ----------------------------------------------------------------------------
// rule
class Rule
{
public:
	Rule() {}

	Rule(std::string &name) { m_name = name; }

	std::string &name() { return m_name; }
	std::string &mod() { return m_mod; }
	std::vector<Elem> &elems() { return m_elems; }

	void clear() { m_elems.clear(); }

	std::string to_string()
	{
		std::string str = m_name + " :";
		for (auto elem : m_elems)
		{
			str += elem.to_string();
		}
		return str;
	}

private:
	std::string m_name;
	std::string m_mod;
	std::vector<Elem> m_elems;
};

// ----------------------------------------------------------------------------
// parent class of a grammar
class Grammar
{
public:
	std::map<std::string, Rule> &rules() { return m_rules; };
	std::string &rule_root() { return m_rule_root; };

	void clear() { m_rules.clear(); };

private:
	std::map<std::string, Rule> m_rules;
	std::string m_rule_root = "";
};

// ----------------------------------------------------------------------------
// IPG parser generator
class ParseGen
{
// private members
private:
	// these chars must be escaped in a character class
	const char *ch_class_reserve_chars = "!-[\\]^";
	// valid escape chars
	const char *esc_chars = "!-[\\]^abfnrtv";

	const char *m_text = nullptr;

	uint32_t m_pos = 0;
	uint32_t m_line = 1;
	uint32_t m_col = 1;

	Grammar m_grammar;

// public methods
public:
	// ------------------------------------------------------------------------
	ParseGen() { m_grammar.clear(); }

	// ------------------------------------------------------------------------
	uint32_t col() { return m_col; }

	// ------------------------------------------------------------------------
	uint32_t line() { return m_line; }

	// ------------------------------------------------------------------------
	void print_rules_debug()
	{
		for (auto rule : m_grammar.rules())
		{
			fprintf(stderr, "%s:", rule.first.c_str());
			for (auto elem : rule.second.elems())
			{
				print_elem_debug(elem);
			}
			fprintf(stderr, "\n");
		}
	}

	// ------------------------------------------------------------------------
	void print_elem_debug(Elem &elem, uint32_t depth = 0)
	{
		if (elem.sub_elems().size() > 0)
		{
			std::string tabs(depth, '\t');
			if (ElemType::ALT == elem.type()) fprintf(stderr, "\n%s|\n", tabs.c_str());
			else if (ElemType::GROUP == elem.type()) fprintf(stderr, "\n%s(\n", tabs.c_str());
			fprintf(stderr, "%s", tabs.c_str());
			for (auto sub_elem : elem.sub_elems())
			{
				print_elem_debug(sub_elem, depth + 1);
			}
			if (ElemType::GROUP == elem.type()) fprintf(stderr, "\n%s)\n", tabs.c_str());
		}
		else
		{
			for (auto str : elem.text())
			{
				fprintf(stderr, " %s", str.c_str());
			}
		}
		fprintf(stderr, "%s", elem.quantifier_strs()[elem.quantifier()].c_str());
	}

	// ------------------------------------------------------------------------
	// NOTE: does only minimal error-checking since input should be well-formed
	//       from parsing grammar
	void print_parser()
	{
		printf(
R"foo(
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

// TODO: replace with enum class
#define RET_FAIL 0
#define RET_OK 1
#define RET_INLINE 2

class ASTNode
{
public:
	ASTNode() {}
	ASTNode(uint32_t pos, std::string text) { m_pos = pos; m_text = text; }
	void clear() { m_pos = 0; m_text.clear(); m_children.clear(); }
	uint32_t pos() { return m_pos; }
	std::string text() { return m_text; }
	void add_child(ASTNode &child) { m_children.push_back(child); }
    ASTNode &child(uint32_t index) { return m_children[index]; }
	std::vector<ASTNode> &children() { return m_children; }
	void print(uint32_t depth = 0)
	{
		printf("%%s%%s", std::string(depth * 2, ' ').c_str(), m_text.c_str());
		if (m_children.size() > 0) printf(": %%lu", m_children.size());
		printf("\n");
		for (auto child : m_children) child.print(depth + 1);
	}
private:
	uint32_t m_pos = 0;
	std::string m_text;
	std::vector<ASTNode> m_children;
};

class Parser
{
private:
	const char *m_text = nullptr;
	uint32_t m_pos = 0;
	uint32_t m_line = 1;
	uint32_t m_col = 1;
	std::vector<uint32_t> m_errs;
public:
	Parser(char *text) { m_text = text; }
	size_t len() { return strlen(m_text); }
	uint32_t col() { return m_col; }
	uint32_t line() { return m_line; }
	uint32_t pos() { return m_pos; }
	void print_last_err()
	{
		fprintf(stderr, "ERROR parsing grammar near position %%s (line %%s column %%s)\n",
			std::to_string(m_errs.end()[-3]).c_str(),
			std::to_string(m_errs.end()[-2]).c_str(),
			std::to_string(m_errs.end()[-1]).c_str());
	}
	void print_errs()
	{
		for (auto err : m_errs)
		{ fprintf(stderr, "E: %%s\n", std::to_string(err).c_str()); }
	}
)foo");

		for (auto rule : m_grammar.rules()) print_rule(rule.second);

		printf(
R"foo(
	// on success, writes extracted code bits to num
	// returns number of bytes from utf-8 on success or -1 on failure
	int32_t utf8_to_int32(int32_t *num, const char *str)
	{
		int32_t n_bytes;
		uint8_t byte = (uint8_t)str[0];
		uint32_t val;
		// in 1 byte case, return immediately
		if ((byte & 0x80) == 0) { *num = (int32_t)byte; return 1; }
		else if ((byte & 0xe0) == 0xc0) { n_bytes = 2; val = byte & 0x1f; }
		else if ((byte & 0xf0) == 0xe0) { n_bytes = 3; val = byte & 0x0f; }
		else if ((byte & 0xf8) == 0xf0) { n_bytes = 4; val = byte & 0x07; }
		else return -1;
		for (int32_t i = 1; i < n_bytes; i++)
		{
			val <<= 6;
			byte = (uint8_t)str[i];
			if ((byte & 0xc0) != 0x80) return -1;
			val += (int32_t)(byte & 0x3f);
		}
		*num = val;
		return n_bytes;
	}
};

int main(int argc, char **argv)
{
	if (argc < 2)
	{
		fprintf(stderr, "Usage: %%s <filename>\n", argv[0]);
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
	int32_t retval = p.parse_%s(astn);
	printf("%%d\n", p.line());
	printf("%%u %%lu\n", p.pos(), p.len());
	astn.print();
	fprintf(stderr, "%%d\n", retval);
	if (RET_FAIL == retval || p.pos() < p.len()) p.print_last_err();
	else fprintf(stderr, "parsed successfully\n");
	delete[] buf;
	return 0;
}
)foo", m_grammar.rule_root().c_str());
	}

	// ------------------------------------------------------------------------
	void print_rule(Rule &rule)
	{
		printf("\n");
		printf("\t// ***RULE*** %s\n", rule.to_string().c_str());
		printf("\tint32_t parse_%s(ASTNode &parent)\n", rule.name().c_str());
		printf("\t{\n");
		if (SCC_DEBUG) printf("\t\tprintf(\"parse_%s()\\n\");\n", rule.name().c_str());
		printf("\t\tuint32_t pos_prev = m_pos;\n");
		printf("\t\tuint32_t col_prev = m_col;\n");
		printf("\t\tuint32_t line_prev = m_line;\n");
		if ("mergeup" == rule.mod())
		{
			printf("\t\tASTNode &astn0 = parent;\n");
		}
		else
		{
			printf("\t\tASTNode astn0(m_pos, \"%s\");\n", rule.name().c_str());
		}
		printf("\n");

		print_alts(rule.elems());

		printf("\n");
		printf("\t\tif (!ok0)\n");
		printf("\t\t{\n");
		printf("\t\t\tm_errs.push_back(m_pos);\n");
		printf("\t\t\tm_errs.push_back(m_line);\n");
		printf("\t\t\tm_errs.push_back(m_col);\n");
		printf("\t\t\tm_pos = pos_prev;\n");
		printf("\t\t\tm_col = col_prev;\n");
		printf("\t\t\tm_line = line_prev;\n");
		printf("\t\t}\n");
		// only add to AST if discard, inline and mergeup modifications not set
		if ("discard" != rule.mod() && "inline" != rule.mod() && "mergeup" != rule.mod())
		{
			printf("\t\telse\n");
			printf("\t\t{\n");
			printf("\t\t\tparent.add_child(astn0);\n");
			printf("\t\t}\n");
		}
		std::string ret_str = "RET_OK";
		if ("inline" == rule.mod()) ret_str = "RET_INLINE";
		printf("\t\tif (ok0) return %s;\n", ret_str.c_str());
		printf("\t\telse return RET_FAIL;\n");
		printf("\t}\n");
	}

	// ------------------------------------------------------------------------
	void print_alts(std::vector<Elem> &elems, uint32_t depth = 0)
	{
		std::string tabs(depth + 2, '\t');
		printf("%s// ***ALTERNATES***\n", tabs.c_str());
		printf("%sbool ok%d = false;\n", tabs.c_str(), depth);
		printf("%suint32_t pos_start%d = m_pos;\n", tabs.c_str(), depth);
		if (depth > 0)
		{
			printf("%sASTNode astn%d(m_pos, \"alts_tmp\");\n", tabs.c_str(), depth);
		}
		printf("%sfor (;;)\n", tabs.c_str());
		printf("%s{\n", tabs.c_str());
		size_t n_elems = elems.size();
		for (size_t e = 0; e < n_elems; e++)
		{
			if (e > 0) printf("\n");
			print_alt(elems[e], depth + 1);
		}
		printf("\n");
		printf("%s\tbreak;\n", tabs.c_str());
		printf("%s}\n", tabs.c_str());
		printf("%sif (!ok%d)\n", tabs.c_str(), depth);
		printf("%s{\n", tabs.c_str());
		printf("%s\tm_errs.push_back(m_pos);\n", tabs.c_str());
		printf("%s\tm_errs.push_back(m_line);\n", tabs.c_str());
		printf("%s\tm_errs.push_back(m_col);\n", tabs.c_str());
		printf("%s}\n", tabs.c_str());
		printf("%selse\n", tabs.c_str());
		printf("%s{\n", tabs.c_str());
		if (SCC_DEBUG)
		{
			printf("%s\tprintf(\"*%%s*\\n\", std::string(&m_text[pos_start%d], m_pos - pos_start%d).c_str());\n", tabs.c_str(), depth, depth);
		}
		if (depth > 0)
		{
			printf("%s\tfor (auto child%d : astn%d.children())\n", tabs.c_str(), depth, depth);
			printf("%s\t{\n", tabs.c_str());
			printf("%s\t\tastn%d.add_child(child%d);\n", tabs.c_str(), depth - 2, depth);
			printf("%s\t}\n", tabs.c_str());
		}
		//~ printf("%s\tm_errs.clear();\n", tabs.c_str());
		printf("%s}\n", tabs.c_str());
	}

	// ------------------------------------------------------------------------
	void print_alt(Elem &elem, uint32_t depth = 0)
	{
		// sanity check
		if (ElemType::ALT != elem.type()) return;

		std::string tabs(depth + 2, '\t');

		printf("%s// ***ALTERNATE***%s\n", tabs.c_str(), elem.to_string().c_str());
		printf("%sfor (;;)\n", tabs.c_str());
		printf("%s{\n", tabs.c_str());
		printf("%s\tbool ok%d = false;\n", tabs.c_str(), depth);
		printf("%s\tuint32_t pos_start%d = m_pos;\n", tabs.c_str(), depth);
		printf("%s\tuint32_t col_start%d = m_col;\n", tabs.c_str(), depth);
		printf("\n");

		size_t n_elems = elem.sub_elems().size();
		for (size_t e = 0; e < n_elems; e++)
		{
			if (e > 0) printf("\n");
			print_elem(elem.sub_elems()[e], depth + 1);
		}

		printf("\n");
		printf("%s\tok%d = true;\n", tabs.c_str(), depth - 1);
		printf("%s\tbreak;\n", tabs.c_str());
		printf("%s}\n", tabs.c_str());
		printf("%sif (ok%d) break;\n", tabs.c_str(), depth - 1);
	}

	// ------------------------------------------------------------------------
	void print_elem(Elem &elem, uint32_t depth = 0)
	{
		// sanity check
		if (ElemType::ALT == elem.type()) return;

		std::string tabs(depth + 2, '\t');

		printf("%s// ***ELEMENT***%s\n", tabs.c_str(), elem.to_string().c_str());

		if (elem.quantifier() == QuantifierType::ZERO_ONE)
		{
			printf("%sok%d = false;\n", tabs.c_str(), depth - 1);
			printf("%sfor (;;)\n", tabs.c_str());
			printf("%s{\n", tabs.c_str());
			printf("%s\tpos_start%d = m_pos;\n", tabs.c_str(), depth - 1);
			print_elem_inner(elem, depth);
			printf("%s\tok%d = true;\n", tabs.c_str(), depth - 1);
			printf("%s\tbreak;\n", tabs.c_str());
			printf("%s}\n", tabs.c_str());
		}
		else if (elem.quantifier() == QuantifierType::ZERO_PLUS)
		{
			printf("%sok%d = false;\n", tabs.c_str(), depth - 1);
			printf("%sfor (;;)\n", tabs.c_str());
			printf("%s{\n", tabs.c_str());
			printf("%s\tpos_start%d = m_pos;\n", tabs.c_str(), depth - 1);
			print_elem_inner(elem, depth);
			printf("%s\tif (ok%d)\n", tabs.c_str(), depth);
			printf("%s\t{\n", tabs.c_str());
			printf("%s\t\tok%d = ok%d;\n", tabs.c_str(), depth - 1, depth);
			printf("%s\t\tcontinue;\n", tabs.c_str());
			printf("%s\t}\n", tabs.c_str());
			printf("%s\tok%d = true;\n", tabs.c_str(), depth - 1);
			printf("%s\tbreak;\n", tabs.c_str());
			printf("%s}\n", tabs.c_str());
		}
		else if (elem.quantifier() == QuantifierType::ONE_PLUS)
		{
			printf("%sok%d = false;\n", tabs.c_str(), depth - 1);
			printf("%sint32_t counter%d = 0;\n", tabs.c_str(), depth);
			printf("%sfor (;;)\n", tabs.c_str());
			printf("%s{\n", tabs.c_str());
			printf("%s\tpos_start%d = m_pos;\n", tabs.c_str(), depth - 1);
			print_elem_inner(elem, depth);
			printf("%s\tif (!ok%d) break;\n", tabs.c_str(), depth);
			printf("%s\tcounter%d++;\n", tabs.c_str(), depth);
			printf("%s}\n", tabs.c_str());
			printf("%sok%d = (counter%d > 0);\n", tabs.c_str(), depth - 1, depth);
		}
		else if (elem.quantifier() == QuantifierType::ONE)
		{
			printf("%sok%d = false;\n", tabs.c_str(), depth - 1);
			printf("%sfor (;;)\n", tabs.c_str());
			printf("%s{\n", tabs.c_str());
			printf("%s\tpos_start%d = m_pos;\n", tabs.c_str(), depth - 1);
			print_elem_inner(elem, depth);
			printf("%s\tok%d = ok%d;\n", tabs.c_str(), depth - 1, depth);
			printf("%s\tbreak;\n", tabs.c_str());
			printf("%s}\n", tabs.c_str());
		}
		else
		{
			uint32_t qt = (uint32_t)elem.quantifier();
			fprintf(stderr, "FATAL ERROR: unsupported quantifier '%u'\n", qt);
			exit(1);
		}

		printf("%sif (!ok%d)\n", tabs.c_str(), depth - 1);
		printf("%s{\n", tabs.c_str());
		printf("%s\tm_pos = pos_start%d;\n", tabs.c_str(), depth - 1);
		printf("%s\tm_col = col_start%d;\n", tabs.c_str(), depth - 1);
		printf("%s\tbreak;\n", tabs.c_str());
		printf("%s}\n", tabs.c_str());
		printf("%sif (!ok%d) break;\n", tabs.c_str(), depth - 1);
	}

	// ------------------------------------------------------------------------
	void print_elem_inner(Elem &elem, uint32_t depth = 0)
	{
		std::string tabs(depth + 3, '\t');

		if (ElemType::NAME == elem.type())
		{
			printf("%sint32_t ok%d = parse_%s(astn%d);\n", tabs.c_str(), depth, elem.text()[0].c_str(), depth - 2);
			if ("inline" == m_grammar.rules()[elem.text()[0]].mod())
			{
				printf("%sif (RET_INLINE == ok%d)\n", tabs.c_str(), depth);
				printf("%s{\n", tabs.c_str());
				printf("%s\tASTNode astn%d(pos_start%d, std::string(&m_text[pos_start%d], m_pos - pos_start%d));\n",
					tabs.c_str(), depth, depth - 1, depth - 1, depth - 1);
				printf("%s\tastn%d.add_child(astn%d);\n", tabs.c_str(), depth - 2, depth);
				printf("%s}\n", tabs.c_str());
			}
		}
		// NOTE: assumes valid expression since parser should have validated
		else if (ElemType::CH_CLASS == elem.type())
		{
			printf("%sbool ok%d = false;\n", tabs.c_str(), depth);
			printf("%sint32_t ch_decoded;\n", tabs.c_str());
			printf("%sint32_t len_item%d = utf8_to_int32(&ch_decoded, &m_text[m_pos]);\n", tabs.c_str(), depth);

			int32_t idx = 1;
			bool flag_negate_all = false;
			int32_t range_ch1;
			int32_t range_ch2;
			bool escaped = false;
			int32_t ch32 = decode_to_int32(&escaped, (char *)elem.text()[idx].c_str());
			if ('^' == ch32 && !escaped)
			{
				flag_negate_all = true;
				idx++;
			}

			// print expression to check if char matches character class
			printf("%sif (len_item%d > 0 && %s(true", tabs.c_str(), depth, flag_negate_all ? "!" : "");

			// negative expressions
			// loop over all tokens except leading and trailing [ ]
			for (int32_t idx2 = idx; idx2 < elem.text().size() - 1;)
			{
				bool flag_is_range = false;

				bool flag_negate = false;
				ch32 = decode_to_int32(&escaped, (char *)elem.text()[idx2].c_str());
				// check for negation
				if ('!' == ch32 && !escaped)
				{
					flag_negate = true;
					idx2++;
				}
				// get first char in range
				range_ch1 = decode_to_int32(&escaped, (char *)elem.text()[idx2].c_str());
				idx2++;
				ch32 = decode_to_int32(&escaped, (char *)elem.text()[idx2].c_str());
				// check for range 
				if ('-' == ch32 && !escaped)
				{
					flag_is_range = true;
					idx2++;
					range_ch2 = decode_to_int32(&escaped, (char *)elem.text()[idx2].c_str());
					idx2++;
				}

				if (flag_negate)
				{
					printf(" && ");

					// print expression for this part of character class
					if (!flag_is_range)
					{
						printf("%s(ch_decoded == %d)", flag_negate ? "!" : "", range_ch1);
					}
					else
					{
						printf("%s(ch_decoded >= %d && ch_decoded <= %d)", flag_negate ? "!" : "", range_ch1, range_ch2);
					}
				}
			}

			// positive expressions
			// loop over all tokens except leading and trailing [ ]
			printf(" && (false");
			for (; idx < elem.text().size() - 1;)
			{
				bool flag_is_range = false;

				bool flag_negate = false;
				ch32 = decode_to_int32(&escaped, (char *)elem.text()[idx].c_str());
				// check for negation
				if ('!' == ch32 && !escaped)
				{
					flag_negate = true;
					idx++;
				}
				// get first char in range
				range_ch1 = decode_to_int32(&escaped, (char *)elem.text()[idx].c_str());
				idx++;
				ch32 = decode_to_int32(&escaped, (char *)elem.text()[idx].c_str());
				// check for range 
				if ('-' == ch32 && !escaped)
				{
					flag_is_range = true;
					idx++;
					range_ch2 = decode_to_int32(&escaped, (char *)elem.text()[idx].c_str());
					idx++;
				}

				if (!flag_negate)
				{
					printf(" || ");

					// print expression for this part of character class
					if (!flag_is_range)
					{
						printf("(ch_decoded == %d)", range_ch1);
					}
					else
					{
						printf("(ch_decoded >= %d && ch_decoded <= %d)", range_ch1, range_ch2);
					}
				}
			}
			printf(")))\n");
			printf("%s{ m_pos += len_item%d; m_col += len_item%d; ok%d = true; }\n", tabs.c_str(), depth, depth, depth);

			printf("%sif (ok%d)\n", tabs.c_str(), depth);
			printf("%s{\n", tabs.c_str());
			printf("%s\tASTNode astn%d(pos_start%d, std::string(&m_text[pos_start%d], m_pos - pos_start%d));\n",
				tabs.c_str(), depth, depth - 1, depth - 1, depth - 1);
			printf("%s\tastn%d.add_child(astn%d);\n", tabs.c_str(), depth - 2, depth);
			printf("%s\tif ('\\n' == ch_decoded)\n", tabs.c_str());
			printf("%s\t{\n", tabs.c_str());
			printf("%s\t\tm_line++;\n", tabs.c_str());
			printf("%s\t\tm_col = 1;\n", tabs.c_str());
			printf("%s\t}\n", tabs.c_str());
			printf("%s}\n", tabs.c_str());
		}
		else if (ElemType::STRING == elem.type())
		{
			printf("%sbool ok%d = false;\n", tabs.c_str(), depth);
			printf("%sconst char *str = %s;\n", tabs.c_str(), elem.text()[0].c_str());
			printf("%sint32_t i = 0;\n", tabs.c_str());
			printf("%sfor (; i < strlen(str) && m_text[m_pos] == str[i]; i++, m_pos++, m_col++);\n", tabs.c_str());
			printf("%sif (i == strlen(str)) ok%d = true;\n", tabs.c_str(), depth);

			printf("%sif (ok%d)\n", tabs.c_str(), depth);
			printf("%s{\n", tabs.c_str());
			printf("%s\tASTNode astn%d(pos_start%d, std::string(&m_text[pos_start%d], m_pos - pos_start%d));\n",
				tabs.c_str(), depth, depth - 1, depth - 1, depth - 1);
			printf("%s\tastn%d.add_child(astn%d);\n", tabs.c_str(), depth - 2, depth);
			printf("%s}\n", tabs.c_str());
		}
		else if (ElemType::GROUP == elem.type())
		{
			printf("%sint32_t len_item%d = -1;\n", tabs.c_str(), depth);
			print_alts(elem.sub_elems(), depth);
		}
		else
		{
			fprintf(stderr, "FATAL ERROR: unsupported element type: %d\n", (int)elem.type());
			exit(1);
		}
	}

	// ------------------------------------------------------------------------
	// check for named elem and/or sub-elems and add to to_visit_new if not
	// already in to_visit or visited
	void check_rule_elems(
		Elem &elem,
		std::map<std::string, bool> &to_visit,
		std::map<std::string, bool> &to_visit_new,
		std::map<std::string, bool> &visited)
	{
		if (ElemType::NAME == elem.type())
		{
			std::string elem_name = elem.text()[0];
			// add to to_visit_new list
			if (to_visit.find(elem_name) == to_visit.end()
				&& visited.find(elem_name) == visited.end())
			{
				to_visit_new[elem_name] = true;
			}
		}
		// if there are sub elems to this elem, loop over them
		if (elem.sub_elems().size() > 0)
		{
			for (auto sub_elem : elem.sub_elems())
			{
				check_rule_elems(sub_elem, to_visit, to_visit_new, visited);
			}
		}
	}

	// ------------------------------------------------------------------------
	// check for:
	//  1) unreachable rules (no usage tracing to root rule)
	//  2) named elems referring to non-existent rules
	bool check_rules()
	{
		bool retval = true;

		std::map<std::string, bool> to_visit;
		std::map<std::string, bool> visited;
		// initialize to_visit map with root rule
		to_visit[m_grammar.rule_root()] = true;
		// loop until to_visit list is empty
		while (to_visit.size() > 0)
		{
			std::map<std::string, bool> to_visit_new;

			// loop over to_visit list
			for (auto rule : to_visit)
			{
				std::string rule_name = rule.first;
				visited[rule_name] = true;

				if (m_grammar.rules().find(rule_name) == m_grammar.rules().end())
				{
					fprintf(stderr, "ERROR: undefined rule '%s'\n", rule_name.c_str());
					retval = false;
				}

				// loop over child elems
				for (int i = 0; i < m_grammar.rules()[rule_name].elems().size(); i++)
				{
					Elem &elem = m_grammar.rules()[rule_name].elems()[i];
					check_rule_elems(elem, to_visit, to_visit_new, visited);
				}
			}
			to_visit.clear();
			for (auto rule : to_visit_new)
			{
				to_visit[rule.first] = true;
			}
			to_visit = to_visit_new;

			// erase visited rules from to_visit list
			for (auto rule : visited)
			{
				std::string rule_name = rule.first;
				auto it = to_visit.find(rule_name);
				if (it != to_visit.end()) to_visit.erase(it);
			}
		}
		// error if not all visited
		if (m_grammar.rules().size() > visited.size())
		{
			// print unreachable rules
			for (auto rule : m_grammar.rules())
			{
				std::string rule_name = rule.first;
				auto it = visited.find(rule_name);
				if (it == visited.end())
				{
					fprintf(stderr, "ERROR: unreachable rule '%s'\n", rule_name.c_str());
				}
			}
			retval = false;
		}

		return retval;
	}

	// ------------------------------------------------------------------------
	// rules : ws (comment ws)* rule+;
	bool parse_grammar(ASTNode &node_w, const char *text_r)
	{
		if (SCC_DEBUG) fprintf(stderr, "parse_grammar %d\n", m_pos);
		node_w.clear();
		m_text = text_r;

		parse_ws();
		uint32_t m_pos_prev;
		do
		{
			m_pos_prev = m_pos;
			parse_comment();
			parse_ws();
		}
		while (m_text[m_pos] != '\0' && m_pos_prev != m_pos);

		while (m_text[m_pos] != '\0')
		{
			bool ok = parse_rule(node_w);
			if (!ok) return false;
		}

		return true;
	}

	// ------------------------------------------------------------------------
	// rule : ws id ws ("discard" | "inline" | "mergeup")? ws ":" ws alts ws ";" ws (comment ws)*;
	bool parse_rule(ASTNode &node_w)
	{
		if (SCC_DEBUG) fprintf(stderr, "parse_rule %d\n", m_pos);
		parse_ws();

		int32_t len_name = parse_id();
		if (len_name <= 0) return false;

		std::string rule_name(&m_text[m_pos - len_name], len_name);
		// first parsed rule is root of grammar
		if ("" == m_grammar.rule_root()) m_grammar.rule_root() = rule_name;

		if (m_grammar.rules().find(rule_name) != m_grammar.rules().end())
		{
			fprintf(stderr, "ERROR: duplicate rule name '%s'\n", rule_name.c_str());
			return false;
		}

		m_grammar.rules()[rule_name] = Rule(rule_name);

		parse_ws();

		// optional rule modifier
		int32_t len_mod = parse_id();
		if (len_mod > 0)
		{
			std::string rule_mod(&m_text[m_pos - len_mod], len_mod);
			if ("discard" != rule_mod && "inline" != rule_mod && "mergeup" != rule_mod) return false;
			m_grammar.rules()[rule_name].mod() = rule_mod;
		}

		parse_ws();

		if (m_text[m_pos] != ':') return false;
		m_pos++;
		m_col++;

		parse_ws();

		int32_t len_alts = parse_alts(m_grammar.rules()[rule_name].elems());
		if (-1 == len_alts) return false;

		parse_ws();

		if (m_text[m_pos] != ';') return false;
		m_pos++;
		m_col++;

		parse_ws();
		uint32_t m_pos_prev;
		do
		{
			m_pos_prev = m_pos;
			parse_comment();
			parse_ws();
		}
		while (m_text[m_pos] != '\0' && m_pos_prev != m_pos);

		if (SCC_DEBUG) fprintf(stderr, "exiting parse_rule %d\n", m_pos);
		return true;
	}

// private methods
private:
	// ------------------------------------------------------------------------
	// ws discard : [ \n\r\t]*;
	// parse and discard whitespace
	void parse_ws()
	{
		if (SCC_DEBUG) fprintf(stderr, "parse_ws\n");
		for (;;)
		{
			char ch = m_text[m_pos];
			if (ch == ' ' || ch == '\t' || ch == '\r')
			{
				m_pos++;
				if (ch != '\r') m_col++;
				continue;
			}
			else if (ch == '\n')
			{
				m_pos++;
				m_line++;
				m_col = 1;
				continue;
			}
			break;
		}
	}

	// ------------------------------------------------------------------------
	// comment discard : "#" [^\r\n]*;
	// parse and discard comment
	void parse_comment()
	{
		if (SCC_DEBUG) fprintf(stderr, "parse_comment\n");
		char ch = m_text[m_pos];
		if (ch != '#') return;
		for (;;)
		{
			m_pos++;
			m_col++;
			ch = m_text[m_pos];
			if (ch == '\r' || ch == '\n' || ch == '\0') break;
		}
	}

	// ------------------------------------------------------------------------
	// id : [A-Za-z][0-9A-Za-z_]*;
	// returns length on success, -1 on failure
	int32_t parse_id()
	{
		if (SCC_DEBUG) fprintf(stderr, "parse_id\n");
		int32_t len = 0;
		char ch = m_text[m_pos];
		if (!((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z'))) return -1;
		m_pos++;
		m_col++;
		len++;
		for (;;)
		{
			ch = m_text[m_pos];
			if (ch == '_' || (ch >= '0' && ch <= '9')
			|| (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z'))
			{
				m_pos++;
				m_col++;
				len++;
				continue;
			}
			return len;
		}
	}

	// ------------------------------------------------------------------------
	// alts : alt (ws "|" ws alt)*;
	// returns length on success, -1 on failure
	int32_t parse_alts(std::vector<Elem> &elems)
	{
		if (SCC_DEBUG) fprintf(stderr, "parse_alts\n");
		int32_t len = 0;
		bool trailing_bar;
		while (m_text[m_pos] != '\0')
		{
			Elem elem_alt(ElemType::ALT);
			int32_t len_el = parse_alt(elem_alt.sub_elems());
			if (-1 == len_el) break;
			len += len_el;
			trailing_bar = false;
			elems.push_back(elem_alt);

			parse_ws();

			char ch = m_text[m_pos];
			if (ch == '|')
			{
				trailing_bar = true;
				m_pos++;
				m_col++;
				len++;

				parse_ws();
				continue;
			}
			break;
		}
		if (len <= 0 || trailing_bar) return -1;
		return len;
	}

	// ------------------------------------------------------------------------
	// alt : elem (ws elem)*;
	// returns length on success, -1 on failure
	int32_t parse_alt(std::vector<Elem> &elems)
	{
		if (SCC_DEBUG) fprintf(stderr, "parse_alt %d\n", m_pos);
		int32_t len = 0;
		while (m_text[m_pos] != '\0')
		{
			int32_t len_el = parse_element(elems);
			if (len_el <= 0) break;
			len += len_el;

			parse_ws();
		}
		if (len <= 0) return -1;
		return len;
	}

	// ------------------------------------------------------------------------
	// elem mergeup: (group | id | ch_class | string) [?*+]?;
	// returns length on success, -1 on failure
	int32_t parse_element(std::vector<Elem> &elems)
	{
		if (SCC_DEBUG) fprintf(stderr, "parse_element\n");

		int32_t len = 0;
		while (m_text[m_pos] != ';' && m_text[m_pos] != '\0')
		{
			int32_t len_item;
			for (int32_t i = 0; i < 1; i++)
			{
				len_item = parse_group(elems);
				if (len_item > 0)
				{
					len += len_item;
					break;
				}
				len_item = parse_id();
				if (len_item > 0)
				{
					std::string name(&m_text[m_pos - len_item], len_item); 
					elems.push_back(Elem(ElemType::NAME, name));
					len += len_item;
					break;
				}
				len_item = parse_ch_class(elems);
				if (len_item > 0)
				{
					len += len_item;
					break;
				}
				len_item = parse_string(elems);
				if (len_item >= 0)
				{
					len += len_item;
					break;
				}
			}

			if (len <= 0) return -1;

			parse_ws();

			char ch = m_text[m_pos];
			if (ch == '?' || ch == '*' || ch == '+')
			{
				QuantifierType qt;
				if (ch == '?') qt = QuantifierType::ZERO_ONE;
				else if (ch == '*') qt = QuantifierType::ZERO_PLUS;
				else if (ch == '+') qt = QuantifierType::ONE_PLUS;
				elems[elems.size() - 1].quantifier() = qt;
				m_pos++;
				m_col++;
				len++;

				parse_ws();
			}

			if (len_item <= 0) break;
		}

		return len;
	}

	// ------------------------------------------------------------------------
	// group : "(" ws alts ws ")";
	// returns length on success, -1 on failure
	int32_t parse_group(std::vector<Elem> &elems)
	{
		if (SCC_DEBUG) fprintf(stderr, "parse_group\n");

		uint32_t pos_prev = m_pos;
		uint32_t col_prev = m_col;
		uint32_t line_prev = m_line;
		int32_t len = 0;

		char ch = m_text[m_pos];
		if (ch != '(')
		{
			pos_prev = m_pos;
			col_prev = m_col;
			line_prev = m_line;
			return -1;
		}
		Elem elem_group(ElemType::GROUP);
		m_pos++;
		m_col++;
		len++;

		parse_ws();

		int32_t len_alts = parse_alts(elem_group.sub_elems());
		if (len_alts < 0)
		{
			pos_prev = m_pos;
			col_prev = m_col;
			line_prev = m_line;
			return -1;
		}
		len += len_alts;

		parse_ws();

		ch = m_text[m_pos];
		if (ch != ')')
		{
			pos_prev = m_pos;
			col_prev = m_col;
			line_prev = m_line;
			return -1;
		}
		elems.push_back(elem_group);
		m_pos++;
		m_col++;
		len++;

		return len;
	}

	// ------------------------------------------------------------------------
	// string : "\"" char* "\"";
	// parse literal string in double quotes
	// returns length on success, -1 on failure
	int32_t parse_string(std::vector<Elem> &elems)
	{
		if (SCC_DEBUG) fprintf(stderr, "parse_string %d\n", m_pos);
		uint32_t pos_prev = m_pos;
		uint32_t col_prev = m_col;
		uint32_t line_prev = m_line;
		char ch = m_text[m_pos];
		if (ch != '"') return -1;
		int32_t len = 1;
		m_pos++;
		m_col++;
		bool esc_set = false;
		for (;; m_pos++, m_col++, len++)
		{
			ch = m_text[m_pos];
			if (ch < ' ') break;
			if (ch == '\\' && !esc_set)
			{
				esc_set = true;
				continue;
			}
			if (ch == '"' && !esc_set)
			{
				m_pos++;
				m_col++;
				len++;
				std::string str(&m_text[m_pos - len], len);
				elems.push_back(Elem(ElemType::STRING, str));
				return len;
			}
			esc_set = false;
		}
		m_pos = pos_prev;
		m_col = col_prev;
		m_line = line_prev;
		return -1;
	}

	// ------------------------------------------------------------------------
	// ch_class : "[" "^"? ch_class_range ("!"? ch_class_range)* "]";
	// parse bracket expression
	// returns length on success, -1 on failure
	int32_t parse_ch_class(std::vector<Elem> &elems)
	{
		if (SCC_DEBUG) fprintf(stderr, "parse_ch_class %d\n", m_pos);

		uint32_t pos_prev = m_pos;
		uint32_t col_prev = m_col;
		uint32_t line_prev = m_line;

		int32_t len = 0;

		char ch = m_text[m_pos];
		if (ch != '[')
		{
			if (SCC_DEBUG) fprintf(stderr, "exiting parse_ch_class %d\n", len);
			return -1;
		}

		Elem elem(ElemType::CH_CLASS);
		elem.text().push_back("[");

		m_pos++;
		m_col++;
		len++;
		ch = m_text[m_pos];

		// optional logical not for entire expression
		if (ch == '^')
		{
			elem.text().push_back("^");
			m_pos++;
			m_col++;
			len++;
			ch = m_text[m_pos];
		}

		// first range is required
		int32_t len_br = parse_ch_class_range(elem);
		if (-1 == len_br)
		{
			m_pos = pos_prev;
			m_col = col_prev;
			m_line = line_prev;
			if (SCC_DEBUG) fprintf(stderr, "exiting parse_ch_class %d\n", len);
			return -1;
		}
		len += len_br;

		// 0 or more additional ranges
		for (;;)
		{
			ch = m_text[m_pos];
			if (ch == ']') break;

			int32_t valid_range_elements = 0;
			int32_t len_range = 0;
			uint32_t pos_prev_range = m_pos;
			uint32_t col_prev_range = m_col;
			uint32_t line_prev_range = m_line;

			// optional logical not for this range
			if (ch == '!')
			{
				elem.text().push_back("!");
				m_pos++;
				m_col++;
				len_range++;
				valid_range_elements++;
			}

			len_br = parse_ch_class_range(elem);
			if (-1 == len_br)
			{
				m_pos = pos_prev_range;
				m_col = col_prev_range;
				m_line = line_prev_range;
				for (int32_t i = 0; i < valid_range_elements; i++)
				{
					elem.text().pop_back();
				}
				break;
			}
			len += len_range + len_br;
		}

		ch = m_text[m_pos];
		if (ch != ']' || len <= 0)
		{
			m_pos = pos_prev;
			m_col = col_prev;
			m_line = line_prev;
			len = -1;
		}
		else
		{
			elem.text().push_back("]");
			elems.push_back(elem);
			m_pos++;
			m_col++;
			len++;
		}

		if (SCC_DEBUG) fprintf(stderr, "exiting parse_ch_class %d\n", len);
		return len;
	}

	// ------------------------------------------------------------------------
	// ch_class_range inline : char ("-" char)?;
	// returns length on success, -1 on failure
	int32_t parse_ch_class_range(Elem &elem)
	{
		if (SCC_DEBUG) fprintf(stderr, "parse_ch_class_range\n");

		int32_t len = 0;

		char ch = m_text[m_pos];
		if (ch == ']') return -1;

		int32_t len_char = parse_char(elem);
		if (len_char < 0) return -1;

		// disallow unescaped reserved character
		if (len_char == 1)
		{
			ch = elem.text()[elem.text().size() - 1][0];
			for (int32_t i = 0; ch_class_reserve_chars[i] != '\0'; i++)
			{
				if (ch == ch_class_reserve_chars[i])
				{
					fprintf(stderr, "###*%c*\n", ch);
					return -1;
				}
			}
		}

		len += len_char;

		ch = m_text[m_pos];
		if (ch != '-') return len;
		elem.text().push_back(std::string(&m_text[m_pos], 1));
		m_pos++;
		m_col++;
		len++;

		// cannot end with trailing '-'
		ch = m_text[m_pos];
		if (ch == ']') return -1;

		len_char = parse_char(elem);
		if (len_char < 0) return -1;

		// disallow unescaped reserved character
		if (len_char == 1)
		{
			for (int32_t i = 0; ch_class_reserve_chars[i] != '\0'; i++)
			{
				if (ch == ch_class_reserve_chars[i]) return -1;
			}
		}

		// error if first element >= second one
		std::string ch1 = elem.text()[1];
		std::string ch2 = elem.text()[3];
		bool escaped = false;
		if (decode_to_int32(&escaped, ch1.c_str()) >= decode_to_int32(&escaped, ch2.c_str()))
		{
			fprintf(stderr, "ERROR: invalid range [%s-%s]: '%s' is not < '%s'\n",
				ch1.c_str(), ch2.c_str(), ch1.c_str(), ch2.c_str());
			return -1;
		}

		len += len_char;

		return len;
	}

	// ------------------------------------------------------------------------
	// char inline : [\u0020-\U0010ffff!'!"!\\] | "\\" esc;
	// esc inline : [!-[\\]^abfnrtv] | unicode;
	// unicode inline : "u" hex hex hex hex | "U00" hex hex hex hex hex hex;
	// hex inline : [0-9A-Fa-f];
	// returns length on success, -1 on failure
	int32_t parse_char(Elem &elem)
	{
		if (SCC_DEBUG) fprintf(stderr, "parse_char\n");
		char ch = m_text[m_pos];
		if (ch >= 0 && ch < ' ') return -1;

		// if high bit set
		if (ch < 0)
		{
			int32_t len = parse_utf8_char();
			if (len < 0) return -1;
			elem.text().push_back(std::string(&m_text[m_pos], len));
			m_pos += len;
			m_col += len;
			return len;
		}

		bool is_esc = false;
		if (ch == '\\')
		{
			is_esc = true;
			m_pos++;
			m_col++;
			ch = m_text[m_pos];
		}
		if (ch >= 0 && ch < ' ') return -1;

		// 1 byte UTF-8 character
		if (!is_esc)
		{
			elem.text().push_back(std::string(&m_text[m_pos], 1));
			m_pos++;
			m_col++;
			return 1;
		}

		// single-char escapes
		for (int32_t i = 0; esc_chars[i] != '\0'; i++)
		{
			if (ch == esc_chars[i])
			{
				m_pos++;
				m_col++;
				elem.text().push_back(std::string(&m_text[m_pos - 2], 2));
				return 2;
			}
		}

		// \u[0-9A-Fa-f]{4}
		if (ch == 'u')
		{
			m_pos++;
			m_col++;
			int32_t i = 0;
			for (; i < 4 && is_hex_char(m_text[m_pos]); i++, m_pos++, m_col++);
			if (i < 4) return -1;
			elem.text().push_back(std::string(&m_text[m_pos - 6], 6));
			return 6;
		}

		// \U[0-9A-Fa-f]{8}
		if (ch == 'U')
		{
			m_pos++;
			m_col++;
			int32_t i = 0;
			for (; i < 8 && is_hex_char(m_text[m_pos]); i++, m_pos++, m_col++);
			if (i < 8) return -1;
			elem.text().push_back(std::string(&m_text[m_pos - 10], 10));
			return 10;
		}

		return -1;
	}

	// ------------------------------------------------------------------------
	// returns length on success, -1 on failure
	int32_t parse_utf8_char()
	{
		int32_t n_bytes;
		uint8_t byte = (uint8_t)m_text[m_pos];
		// in 1 byte case, return immediately; all others must be validated
		if ((byte & 0x80) == 0) return 1;
		else if ((byte & 0xe0) == 0xc0) n_bytes = 2;
		else if ((byte & 0xf0) == 0xe0) n_bytes = 3;
		else if ((byte & 0xf8) == 0xf0) n_bytes = 4;
		else return -1;
		for (int32_t i = 0; i < n_bytes; i++)
		{
			byte = (uint8_t)m_text[m_pos + i];
			if ((byte & 0xc0) != 0xc0) return -1;
		}
		return n_bytes;
	}

	// ------------------------------------------------------------------------
	// returns extracted code bits from utf-8 on success or -1 on failure
	int32_t utf8_to_int32(const char *str)
	{
		int32_t n_bytes;
		uint8_t byte = (uint8_t)str[0];
		uint32_t val;
		// in 1 byte case, return immediately
		if ((byte & 0x80) == 0) return (int32_t)byte;
		else if ((byte & 0xe0) == 0xc0) { n_bytes = 2; val = byte & 0x1f; }
		else if ((byte & 0xf0) == 0xe0) { n_bytes = 3; val = byte & 0x0f; }
		else if ((byte & 0xf8) == 0xf0) { n_bytes = 4; val = byte & 0x07; }
		else return -1;
		for (int32_t i = 1; i < n_bytes; i++)
		{
			val <<= 6;
			byte = (uint8_t)str[i];
			if ((byte & 0xc0) != 0x80) return -1;
			val += (int32_t)(byte & 0x3f);
		}
		return val;
	}

	// ------------------------------------------------------------------------
	// converts escape sequence string to int32_t and returns on success,
	// returns -1 on failure
	//
	// esc_seq : '\\' esc;
	// esc inline : [\!\-\[\\\]\^abfnrtv] | unicode;
	// unicode inline : "u" hex hex hex hex | "U00" hex hex hex hex hex hex;
	// hex inline : [0-9A-Fa-f];
	int32_t esc_to_int32(const char *str)
	{
		uint32_t i = 0;
		if (str[i] != '\\') return -1;
		char ch = str[i + 1];
		if (ch == 'a') return 0x7;
		else if (ch == 'b') return 0x8;
		else if (ch == 'f') return 0xc;
		else if (ch == 'n') return 0xa;
		else if (ch == 'r') return 0xd;
		else if (ch == 't') return 0x9;
		else if (ch == 'v') return 0xb;
		else if (ch == '!') return 0x21;
		else if (ch == '"') return 0x22;
		//~ else if (ch == '\'') return 0x27;
		else if (ch == '-') return 0x2d;
		//~ else if (ch == '?') return 0x3f;
		else if (ch == '[') return 0x5b;
		else if (ch == '\\') return 0x5c;
		else if (ch == ']') return 0x5d;
		else if (ch == '^') return 0x5e;
		else if (ch == 'u') return hex_to_int32(&str[i + 2], 4);
		else if (ch == 'U') return hex_to_int32(&str[i + 2], 8);
		return -1;
	}

	// ------------------------------------------------------------------------
	int32_t decode_to_int32(bool *escaped, const char *str)
	{
		if (str[0] == '\\')
		{
			*escaped = true;
			return esc_to_int32(str);
		}
	*escaped = false;
		return utf8_to_int32(str);
	}

	// ------------------------------------------------------------------------
	// is character in [0-9A-F]
	bool is_hex_char(char ch)
	{
		return ((ch >= '0' && ch <= '9') || (ch >= 'A' && ch <= 'F')
				|| (ch >= 'a' && ch <= 'f'));
	}

	// ------------------------------------------------------------------------
	// convert hex string of 4 or 8 characters to int32_t
	// returns value on success or -1 on failure
	int32_t hex_to_int32(const char *str, int32_t len)
	{
		if (len != 4 && len != 8) return -1;
		int32_t val = 0;
		for (int32_t i = 0; i < len; i++)
		{
			val <<= 4;
			char ch = str[i];
			if (ch >= '0' && ch <= '9') val += ch - '0';
			else if (ch >= 'A' && ch <= 'F') val += ch - 'A' + 10;
			else if (ch >= 'a' && ch <= 'f') val += ch - 'a' + 10;
			else return -1;
		}
		return val;
	}
};
};

// ----------------------------------------------------------------------------
int main(int argc, char **argv)
{
	IPG::ParseGen pg;
	IPG::ASTNode node;

	if (argc < 2)
	{
		fprintf(stderr, "Usage: %s <grammer_file>\n", argv[0]);
		return 1;
	}

	FILE *fp;
	fp = fopen(argv[1], "rb");
	if (nullptr == fp)
	{
		fprintf(stderr, "ERROR opening file '%s'\n", argv[1]);
		return 1;
	}
	fseek(fp, 0, SEEK_END);
	size_t file_len = ftell(fp);
	fprintf(stderr, "file_len = %lu\n", file_len);
	fseek(fp, 0, SEEK_SET);
	char *buf = new char[file_len + 1];
	buf[file_len] = '\0';
	size_t bytes_read = fread(buf, 1, file_len, fp);
	fclose(fp);
	if (bytes_read != file_len)
	{
		fprintf(stderr, "ERROR reading file\n");
		return 1;
	}
	fprintf(stderr, "read: %lu\n", bytes_read);

	bool ok = pg.parse_grammar(node, buf);
	if (ok) ok = pg.check_rules();
	if (ok)
	{
		pg.print_parser();
		pg.print_rules_debug();
		fprintf(stderr, "parsed successfully\n");
	}
	else
	{
		fprintf(stderr, "ERROR parsing grammar near line %u, col %u\n",
			pg.line(), pg.col());
	}

	delete[] buf;
	return 0;
}
