5;
!5;
((!5 * 4) << 2) > 6;
~5 & 21;

uint8 a = 1234, b = 5;
sint16 a = 1234, b = 5;

sint16 c = ((5));

uint8 d = 5 + (2 * 3);

bool what1 = true;
bool what2 = false;

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
loop (a = b - 7 * 5, c = d + 1, x = 5;;) {}
loop (int32 a = b, c = d + 1, x = 5;;) {}
loop (SomeClass a = 1, b = 2, c = 3;;) {}

vector<sint32> foo1 = [];
vector<sint32> foo2 = [ 5 ];
vector<sint32> foo3 = [5,];
vector<sint32> foo4 = [5,6];


vector<sint32> foo2 = [2+7, asdf ];

map<uint32, customType > bar = 1;  # parser doesn't do type checking so this is ok during parsing step of compilation
map< uint32, vector < customType>> bar = {};
map< uint32, vector < customType>> bar = {};
map< uint32, vector < customType>> bar = {1:1};
map< uint32, vector < customType>> bar = { 1 : 1, asdf : 5 + 7, };

void aVoidFunc()
{
  int32 a = 57;
  a += 5;
}

uint32 aParameterlessFunc()
{
  sint32 asdf = 0;
  loop (sint32 i = 0; i < 10; i += 1)
  {
    asdf += 7;
  }
  return (uint32)asdf;
}

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

bool didItMatch = "a" =~ /(a*b+c[^d])|asdf|(x)/;
/a*b+c[^d]|asdf|(xs*a)/;
/a*b+c[^d]|asdf|(xs+|(a|[^a-zA-Z])|b?c|d*)/;

class MyClass
{
  public SomeType someVar = 1;
  protected sint32 anotherVar = 5;
  private sint32 yetAnotherVar = 7;

  public uint32 myFunc1()
  {
    return 5;
  }

  protected uint32 myFunc1(uint32 foo)
  {
    return 1;
  }

  private uint32 myFunc3(uint32 foo, sint8 bar)
  {
    return 3;
  }
}
