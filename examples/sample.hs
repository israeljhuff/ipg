uint8 a = 1234, b = 5;
sint16 a = 1234, b = 5;

sint16 c = ((5));

uint8 d = 5 + (2 * 3);

{
  if (foo > 10)
  {
    a += 5;
  }
  b += 5;
}

{
  uint64 a = 1234, b = 5;
}
{
  sint32 a = 1234, b = 5;
}

{
  bool x = -32 & !a;
  sint32 y = 45 + ~23;
}

if (7 * a / x != 5 % b)
{
}

if (a == b)
{
  if (foo > 10)
  {
    a += 5;
  }
  b += 5;
}
elif (2 + 4 <= 17 - 7 / !8 * a)
{
}
else
{
}

loop
{
  uint8 a = 1234, b = 5;
  asdf a = 1234, b = 5;  # declaration using custom type (meaningful once classes are implemented)
  a += 6;
  break;
}

loop post
{
  x = y;
  continue;
}

loop (;;) {}

loop (a = b, c = d + 1;;) {}

vector<sint32> foo1 = [];
vector<sint32> foo2 = [ 5 ];
vector<sint32> foo2 = [5,];


vector<sint32> foo2 = [2+7, asdf ];

map<uint32, customType > bar = 1;  # parser doesn't do type checking so this is ok during parsing step of compilation
map< uint32, vector < customType>> bar = {};
map< uint32, vector < customType>> bar = {};
map< uint32, vector < customType>> bar = {1:1};
map< uint32, vector < customType>> bar = { 1 : 1, asdf : 5 + 7, };

sint32 myfunc(uint32 foo, uint8 bar)
{
  if (foo > (SomeClass)!(10 + 7))
  {
    continue;
    break;
    break !(bool)1;
    return;
    return (sint32)10;
  }
  return bar + 10;
}

5;
!5;
