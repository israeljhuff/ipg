#ifndef ASTNode_h
#define ASTNode_h

#include "utils.h"

namespace IPG
{
// ----------------------------------------------------------------------------
// abstract syntax tree node
class ASTNode
{
public:
	ASTNode() {}
	ASTNode(uint32_t pos, uint32_t line, uint32_t col, std::string text)
	{
		m_pos = pos;
		m_line = line;
		m_col = col;
		m_text = text;
	}
	void clear()
	{
		m_pos = 0;
		m_line = 1;
		m_col = 1;
		m_text.clear();
		m_children.clear();
	}
	uint32_t pos() { return m_pos; }
	uint32_t line() { return m_line; }
	uint32_t col() { return m_col; }
	std::string text() { return m_text; }
	void add_child(ASTNode &child) { m_children.push_back(child); }
    ASTNode &child(uint32_t index) { return m_children[index]; }
	std::vector<ASTNode> &children() { return m_children; }
	void print(uint32_t depth = 0)
	{
		prints(std::string(depth * 2, ' '), m_text);
		if (m_children.size() > 0) prints(": ", m_children.size(), " ", m_pos, " ", m_line, " ", m_col);
		println("");
		for (auto child : m_children) child.print(depth + 1);
	}
private:
	uint32_t m_pos = 0;
	uint32_t m_line = 1;
	uint32_t m_col = 1;
	std::string m_text;
	std::vector<ASTNode> m_children;
};
};

#endif
