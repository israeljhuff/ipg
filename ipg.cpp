// Israel's Parser Generator (IPG)
// Author: Israel Huff
// https://github.com/israeljhuff/ipg
//
// to build on Linux or Windows (Cygwin):
//  g++ --std=c++11 ipg.cpp -o ipg.exe && ./ipg.exe ipg.grammar > tmp.cpp
//  g++ --std=c++11 tmp.cpp -o tmp.exe && ./tmp.exe tmp.grammar

// TODO: vector instead of map for rule list?
// TODO: track names of last fully- and partially-parsed rules
// TODO: clean up output code and reduce redundancy if/where possible
// TODO: should generated class name be user-configurable instead of always "Parser"?

#include <cstdio>
#include <map>
#include <vector>

#include "ASTNode.h"

#define SCC_DEBUG 0

using namespace IPG;

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
			eprints(rule.first, ":");
			for (auto elem : rule.second.elems())
			{
				print_elem_debug(elem);
			}
			eprintln("");
		}
	}

	// ------------------------------------------------------------------------
	void print_elem_debug(Elem &elem, uint32_t depth = 0)
	{
		if (elem.sub_elems().size() > 0)
		{
			std::string tabs(depth, '\t');
			if (ElemType::ALT == elem.type()) eprintln("\n", tabs, "|");
			else if (ElemType::GROUP == elem.type()) eprintln("\n", tabs, "(");
			eprints(tabs);
			for (auto sub_elem : elem.sub_elems())
			{
				print_elem_debug(sub_elem, depth + 1);
			}
			if (ElemType::GROUP == elem.type()) eprintln("\n", tabs, ")");
		}
		else
		{
			for (auto str : elem.text())
			{
				eprints(" ", str);
			}
		}
		eprints(elem.quantifier_strs()[elem.quantifier()]);
	}

	// ------------------------------------------------------------------------
	// NOTE: does only minimal error-checking since input should be well-formed
	//       from parsing grammar
	void print_parser()
	{
		prints(
R"foo(#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "ASTNode.h"

// TODO: replace with enum class
#define RET_FAIL 0
#define RET_OK 1
#define RET_INLINE 2

namespace IPG
{
class Parser
{
private:
	const char *m_text = nullptr;
	uint32_t m_pos = 0;
	uint32_t m_line = 1;
	uint32_t m_col = 1;
	uint32_t m_pos_ok = 0;
	uint32_t m_line_ok = 1;
	uint32_t m_col_ok = 1;
public:
	Parser(char *text) { m_text = text; }
	size_t len() { return strlen(m_text); }
	uint32_t col() { return m_col; }
	uint32_t line() { return m_line; }
	uint32_t pos() { return m_pos; }
	uint32_t col_ok() { return m_col_ok; }
	uint32_t line_ok() { return m_line_ok; }
	uint32_t pos_ok() { return m_pos_ok; }
)foo");
		println("int32_t parse(ASTNode &parent) { return parse_", m_grammar.rule_root(), "(parent); }");

		for (auto rule : m_grammar.rules()) print_rule(rule.second);

		println("};");
		println("};");
	}

	// ------------------------------------------------------------------------
	void print_rule(Rule &rule)
	{
		println("");
		println("\t// ***RULE*** ", rule.to_string());
		println("\tint32_t parse_", rule.name(), "(ASTNode &parent)");
		println("\t{");
		if (SCC_DEBUG) println("\t\tprintln(\"parse_", rule.name(), "()\");");
		println("\t\tuint32_t pos_prev = m_pos;");
		println("\t\tuint32_t line_prev = m_line;");
		println("\t\tuint32_t col_prev = m_col;");
		if ("mergeup" == rule.mod())
		{
			println("\t\tASTNode &astn0 = parent;");
		}
		else
		{
			println("\t\tASTNode astn0(m_pos, \"", rule.name(), "\");");
		}
		println("");

		print_alts(rule.elems());

		println("");
		println("\t\tif (!ok0)");
		println("\t\t{");
		println("\t\t\tm_pos = pos_prev;");
		println("\t\t\tm_line = line_prev;");
		println("\t\t\tm_col = col_prev;");
		println("\t\t}");
		// only add to AST if discard, inline and mergeup modifications not set
		if ("discard" != rule.mod() && "inline" != rule.mod() && "mergeup" != rule.mod())
		{
			println("\t\telse");
			println("\t\t{");
			println("\t\t\tparent.add_child(astn0);");
			println("\t\t}");
		}
		std::string ret_str = "RET_OK";
		if ("inline" == rule.mod()) ret_str = "RET_INLINE";
		println("\t\tif (ok0) return ", ret_str, ";");
		println("\t\telse return RET_FAIL;");
		println("\t}");
	}

	// ------------------------------------------------------------------------
	void print_alts(std::vector<Elem> &elems, uint32_t depth = 0)
	{
		std::string tabs(depth + 2, '\t');
		println(tabs, "// ***ALTERNATES***");
		println(tabs, "bool ok", depth, " = false;");
		println(tabs, "uint32_t pos_start", depth, " = m_pos;");
		println(tabs, "uint32_t line_start", depth, " = m_line;");
		println(tabs, "uint32_t col_start", depth, " = m_col;");
		if (depth > 0)
		{
			println(tabs, "ASTNode astn", depth, "(m_pos, \"alts_tmp\");");
		}
		println(tabs, "for (;;)");
		println(tabs, "{");
		size_t n_elems = elems.size();
		for (size_t e = 0; e < n_elems; e++)
		{
			if (e > 0) println("");
			print_alt(elems[e], depth + 1);
			println(tabs, "\tif (ok", depth, ") break;");
		}
		println("");
		println(tabs, "\tbreak;");
		println(tabs, "}");
		println(tabs, "if (!ok", depth, ")");
		println(tabs, "{");
		println(tabs, "\tm_pos = pos_start", depth, ";");
		println(tabs, "\tm_line = line_start", depth, ";");
		println(tabs, "\tm_col = col_start", depth, ";");
		//~ if (depth > 0) println(tabs, "\tbreak;");
		println(tabs, "}");
		println(tabs, "else");
		println(tabs, "{");
		if (SCC_DEBUG)
		{
			println(tabs, "\tprintln(\"*\", std::string(&m_text[pos_start", depth, "], m_pos - pos_start", depth, "), \"*\");");
		}
		if (depth > 0)
		{
			println(tabs, "\tfor (auto child", depth, " : astn", depth, ".children())");
			println(tabs, "\t{");
			println(tabs, "\t\tastn", depth - 2, ".add_child(child", depth, ");");
			println(tabs, "\t}");
		}
		println(tabs, "}");
	}

	// ------------------------------------------------------------------------
	void print_alt(Elem &elem, uint32_t depth = 0)
	{
		// sanity check
		if (ElemType::ALT != elem.type()) return;

		std::string tabs(depth + 2, '\t');

		println(tabs, "// ***ALTERNATE***", elem.to_string());
		println(tabs, "for (;;)");
		println(tabs, "{");
		println(tabs, "\tbool ok", depth , " = false;");
		println(tabs, "\tuint32_t pos_start", depth , " = m_pos;");
		println(tabs, "\tuint32_t line_start", depth , " = m_line;");
		println(tabs, "\tuint32_t col_start", depth , " = m_col;");
		println("");

		size_t n_elems = elem.sub_elems().size();
		for (size_t e = 0; e < n_elems; e++)
		{
			if (e > 0) println("");
			print_elem(elem.sub_elems()[e], depth + 1);
		}

		println("");
		//~ println(tabs, "\tok", depth - 1, " = true;");
		println(tabs, "\tok", depth - 1 ," = ok", depth, ";");
		println(tabs, "\tbreak;");
		println(tabs, "}");
	}

	// ------------------------------------------------------------------------
	void print_elem(Elem &elem, uint32_t depth = 0)
	{
		// sanity check
		if (ElemType::ALT == elem.type()) return;

		std::string tabs(depth + 2, '\t');

		println(tabs, "// ***ELEMENT***", elem.to_string());

		if (elem.quantifier() == QuantifierType::ZERO_ONE)
		{
			println(tabs, "ok", depth - 1, " = false;");
			println(tabs, "for (;;)");
			println(tabs, "{");
			println(tabs, "\tpos_start", depth - 1, " = m_pos;");
			print_elem_inner(elem, depth);
			println(tabs, "\tok", depth - 1, " = true;");
			println(tabs, "\tbreak;");
			println(tabs, "}");
		}
		else if (elem.quantifier() == QuantifierType::ZERO_PLUS)
		{
			println(tabs, "ok", depth - 1, " = false;");
			println(tabs, "for (;;)");
			println(tabs, "{");
			println(tabs, "\tpos_start", depth - 1, " = m_pos;");
			print_elem_inner(elem, depth);
			println(tabs, "\tif (ok", depth, ") continue;");
			println(tabs, "\tok", depth - 1, " = true;");
			println(tabs, "\tbreak;");
			println(tabs, "}");
		}
		else if (elem.quantifier() == QuantifierType::ONE_PLUS)
		{
			println(tabs, "ok", depth - 1, " = false;");
			println(tabs, "int32_t counter", depth, " = 0;");
			println(tabs, "for (;;)");
			println(tabs, "{");
			println(tabs, "\tpos_start", depth - 1, " = m_pos;");
			print_elem_inner(elem, depth);
			println(tabs, "\tif (!ok", depth, ") break;");
			println(tabs, "\tcounter", depth, "++;");
			println(tabs, "}");
			println(tabs, "ok", depth - 1, " = (counter", depth, " > 0);");
		}
		else if (elem.quantifier() == QuantifierType::ONE)
		{
			println(tabs, "ok", depth - 1, " = false;");
			println(tabs, "for (;;)");
			println(tabs, "{");
			println(tabs, "\tpos_start", depth - 1, " = m_pos;");
			print_elem_inner(elem, depth);
			println(tabs, "\tok", depth - 1, " = ok", depth, ";");
			println(tabs, "\tbreak;");
			println(tabs, "}");
		}
		else
		{
			uint32_t qt = (uint32_t)elem.quantifier();
			eprintln("FATAL ERROR: unsupported quantifier '", qt, "'");
			exit(1);
		}

		println(tabs, "if (!ok", depth - 1, ")");
		println(tabs, "{");
		println(tabs, "\tm_pos = pos_start", depth - 1, ";");
		println(tabs, "\tm_line = line_start", depth - 1, ";");
		println(tabs, "\tm_col = col_start", depth - 1, ";");
		println(tabs, "\tbreak;");
		println(tabs, "}");
		println(tabs, "else");
		println(tabs, "{");
		println(tabs, "\tif (m_pos > m_pos_ok)");
		println(tabs, "\t{");
		println(tabs, "\t\tm_pos_ok = m_pos;");
		println(tabs, "\t\tm_line_ok = m_line;");
		println(tabs, "\t\tm_col_ok = m_col;");
		println(tabs, "\t}");
		println(tabs, "}");
	}

	// ------------------------------------------------------------------------
	void print_elem_inner(Elem &elem, uint32_t depth = 0)
	{
		std::string tabs(depth + 3, '\t');

		if (ElemType::NAME == elem.type())
		{
			println(tabs, "int32_t ok", depth, " = parse_", elem.text()[0], "(astn", depth - 2, ");");
			if ("inline" == m_grammar.rules()[elem.text()[0]].mod())
			{
				println(tabs, "if (RET_INLINE == ok", depth, ")");
				println(tabs, "{");
				println(tabs, "\tASTNode astn", depth, "(pos_start", depth - 1,
					", std::string(&m_text[pos_start", depth - 1, "], m_pos - pos_start", depth - 1, "));");
				println(tabs, "\tastn", depth - 2, ".add_child(astn", depth, ");");
				println(tabs, "}");
			}
		}
		// NOTE: assumes valid expression since parser should have validated
		else if (ElemType::CH_CLASS == elem.type())
		{
			println(tabs, "bool ok", depth, " = false;");
			println(tabs, "int32_t ch_decoded;");
			println(tabs, "int32_t len_item", depth, " = utf8_to_int32(&ch_decoded, &m_text[m_pos]);");

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
			prints(tabs, "if (len_item", depth, " > 0 && ", (flag_negate_all ? "!" : ""), "(true");

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
				// check for range separator
				if ('-' == ch32 && !escaped)
				{
					flag_is_range = true;
					idx2++;
					range_ch2 = decode_to_int32(&escaped, (char *)elem.text()[idx2].c_str());
					idx2++;
				}

				if (flag_negate)
				{
					prints(" && ");

					// print expression for this part of character class
					if (!flag_is_range)
					{
						prints((flag_negate ? "!" : ""), "(ch_decoded == ", range_ch1, ")");
					}
					else
					{
						prints((flag_negate ? "!" : ""), "(ch_decoded >= ", range_ch1,
							" && ch_decoded <= ", range_ch2, ")");
					}
				}
			}

			// positive expressions
			// loop over all tokens except leading and trailing [ ]
			prints(" && (false");
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
				// check for range separator
				if ('-' == ch32 && !escaped)
				{
					flag_is_range = true;
					idx++;
					range_ch2 = decode_to_int32(&escaped, (char *)elem.text()[idx].c_str());
					idx++;
				}

				if (!flag_negate)
				{
					prints(" || ");

					// print expression for this part of character class
					if (!flag_is_range)
					{
						prints("(ch_decoded == ", range_ch1, ")");
					}
					else
					{
						prints("(ch_decoded >= ", range_ch1, " && ch_decoded <= ", range_ch2, ")");
					}
				}
			}
			println(")))");
			println(tabs, "{ m_pos += len_item", depth, "; m_col += len_item", depth, "; ok", depth, " = true; }");

			println(tabs, "if (ok", depth, ")");
			println(tabs, "{");
			println(tabs, "\tASTNode astn", depth, "(pos_start", depth - 1,
				", std::string(&m_text[pos_start", depth - 1, "], m_pos - pos_start", depth - 1, "));");
			println(tabs, "\tastn", depth - 2, ".add_child(astn", depth, ");");
			println(tabs, "\tif ('\\n' == ch_decoded)");
			println(tabs, "\t{");
			println(tabs, "\t\tm_line++;");
			println(tabs, "\t\tm_col = 1;");
			println(tabs, "\t}");
			println(tabs, "}");
		}
		else if (ElemType::STRING == elem.type())
		{
			println(tabs, "bool ok", depth, " = false;");
			println(tabs, "const char *str = ", elem.text()[0], ";");
			println(tabs, "int32_t i = 0;");
			println(tabs, "for (; i < strlen(str) && m_text[m_pos] == str[i]; i++, m_pos++, m_col++);");
			println(tabs, "if (i == strlen(str)) ok", depth, " = true;");

			println(tabs, "if (ok", depth, ")");
			println(tabs, "{");
			println(tabs, "\tASTNode astn", depth, "(pos_start", depth - 1,
				", std::string(&m_text[pos_start", depth - 1, "], m_pos - pos_start", depth - 1, "));");
			println(tabs, "\tastn", depth - 2, ".add_child(astn", depth, ");");
			println(tabs, "}");
		}
		else if (ElemType::GROUP == elem.type())
		{
			println(tabs, "int32_t len_item", depth, " = -1;");
			print_alts(elem.sub_elems(), depth);
		}
		else
		{
			eprintln("FATAL ERROR: unsupported element type: ", (int)elem.type());
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
					eprintln("ERROR: undefined rule '", rule_name, "'");
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
					eprintln("ERROR: unreachable rule '", rule_name, "'");
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
		if (SCC_DEBUG) eprintln("parse_grammar ", m_pos);
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
		if (SCC_DEBUG) eprintln("parse_rule ", m_pos);
		parse_ws();

		int32_t len_name = parse_id();
		if (len_name <= 0) return false;

		std::string rule_name(&m_text[m_pos - len_name], len_name);
		// first parsed rule is root of grammar
		if ("" == m_grammar.rule_root()) m_grammar.rule_root() = rule_name;

		if (m_grammar.rules().find(rule_name) != m_grammar.rules().end())
		{
			eprintln("ERROR: duplicate rule name '", rule_name, "'");
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

		if (SCC_DEBUG) eprintln("exiting parse_rule ", m_pos);
		return true;
	}

// private methods
private:
	// ------------------------------------------------------------------------
	// ws discard : [ \n\r\t]*;
	// parse and discard whitespace
	void parse_ws()
	{
		if (SCC_DEBUG) eprintln("parse_ws");
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
		if (SCC_DEBUG) eprintln("parse_comment");
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
		if (SCC_DEBUG) eprintln("parse_id");
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
		if (SCC_DEBUG) eprintln("parse_alts");
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
		if (SCC_DEBUG) eprintln("parse_alt ", m_pos);
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
		if (SCC_DEBUG) eprintln("parse_element");

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
		if (SCC_DEBUG) eprintln("parse_group");

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
		if (SCC_DEBUG) eprintln("parse_string ", m_pos);
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
		if (SCC_DEBUG) eprintln("parse_ch_class ", m_pos);

		uint32_t pos_prev = m_pos;
		uint32_t col_prev = m_col;
		uint32_t line_prev = m_line;

		int32_t len = 0;

		char ch = m_text[m_pos];
		if (ch != '[')
		{
			if (SCC_DEBUG) eprintln("exiting parse_ch_class ", len);
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
			if (SCC_DEBUG) eprintln("exiting parse_ch_class ", len);
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

		if (SCC_DEBUG) eprintln("exiting parse_ch_class ", len);
		return len;
	}

	// ------------------------------------------------------------------------
	// ch_class_range inline : char ("-" char)?;
	// returns length on success, -1 on failure
	int32_t parse_ch_class_range(Elem &elem)
	{
		if (SCC_DEBUG) eprintln("parse_ch_class_range");

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
					eprintln("###*", ch, "*");
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
			eprintln("ERROR: invalid range [", ch1, "-", ch2, "]: '", ch1, "' >= '", ch2, "'. left char must be < right char");
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
		if (SCC_DEBUG) eprintln("parse_char");
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
		int32_t val;
		int32_t len_item = utf8_to_int32(&val, str);
		return (len_item > 0) ? val : -1;
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
		eprintln("Usage: ", argv[0], " <grammer_file>");
		return 1;
	}

	FILE *fp;
	fp = fopen(argv[1], "rb");
	if (nullptr == fp)
	{
		eprintln("ERROR opening file '", argv[1], "'");
		return 1;
	}
	fseek(fp, 0, SEEK_END);
	size_t file_len = ftell(fp);
	eprintln("file_len = ", file_len);
	fseek(fp, 0, SEEK_SET);
	char *buf = new char[file_len + 1];
	buf[file_len] = '\0';
	size_t bytes_read = fread(buf, 1, file_len, fp);
	fclose(fp);
	if (bytes_read != file_len)
	{
		eprintln("ERROR reading file");
		return 1;
	}
	eprintln("read: ", bytes_read);

	bool ok = pg.parse_grammar(node, buf);
	if (ok) ok = pg.check_rules();
	if (ok)
	{
		pg.print_parser();
		pg.print_rules_debug();
		eprintln("parsed successfully");
	}
	else
	{
		eprintln("ERROR parsing grammar near line ", pg.line(), ", col ", pg.col());
	}

	delete[] buf;
	return 0;
}
