hello();
5;
!5;

a += 1, b += 1;

loop (fp32 b = 2.5; a < 5 && b <= 4.5; a += 1, b += 0.5)
{
}

loop ({ sint32 a = 1; fp32 b = 2.5; }; a < 5 && b <= 4.5; a += 1, b += 0.5)
{
}

loop post ({ sint32 a = 1; fp32 b = 2.5; }; a < 5 && b <= 4.5; {a += 1, b += 0.5; someTask(); })
{
}

{
  dummy();
  5 + 7;
#  id (foo > 10 || bar < 5) {}
#  print("asdf", 3,);
#  void fft_1d_no_recurse(vector<fp32> real, vector<fp32> imag, sint32 log2N,) {}
}

print();
print("asdf");
print("asdf", 1);

doSomething(5, 2, doSomethingElse(), (1 + 2), 12 + 6);

myObj.myMethod();
myObj.myMethod("hi");
myObj.myMethod("hi", 2);

myObj.myMemberObj.myMethod("hi", 2);

myObj.myMemberObj.myMemberObj2.myMethod("hi", 2);

((!5 * 4) << 2) > 6;
~5 & 21;

uint8 a = 1234, b = 5;
sint16 a = 1234, b = 5;

sint16 c = ((5));

uint8 d = 5 + (2 * 3);


bool what1 = true;
bool what2 = false;

{
  if (foo > 10 || bar < 5)
  {
    a += 5;
  }
  b += 5;
}

real[idx] = real[i];
real[(a + 5) & 3] = real[(i + 7) ^ 5] + 1;
real[a][3][!1] = real[b][~3][2] / foo[2];

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
loop (a += 1;;) {}
loop (a += 1, b += 1;;) {}
loop (a = b - 7 * 5, c = d + 1, x = 5;;) {}
loop (int32 a = b, c = d + 1, x = 5;;) {}
loop (SomeClass a = 1, b = 2, c = 3;;) {}
loop (int32 a = 1, b = 2, c = 3; a < 10; a += 1, b += 1, c += 1) {}

vector<sint32> foo1 = [];
vector<sint32> foo2 = [ 5 ];
vector<sint32> foo3 = [5,];
vector<sint32> foo4 = [5,6];
vector<sint32> foo2 = [2+7, asdf ];

map<uint32, customType > bar = 1;  # parser doesn't do type checking so this is ok during parsing step of compilation
map< uint32, vector < customType>> bar = [];
map< uint32, vector < customType>> bar = [];
map< uint32, vector < customType>> bar = [1:1];
map< uint32, vector < customType>> bar = [ 1 : 1, asdf : 5 + 7, ];

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

class SomeParentClass
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

class SomeChildClass : SomeParentClass
{
  private sint32 asdf = 2;
  private string someMethod() {}
}

class FFT
{
  private fp32 tmp1 = 1;
  private vector<fp32> tmp2 = [], tmp3 = [];

  #------------------------------------------------------------------------------
  # NOTE: cos_tbl and sin_tbl LUTs assumed to be 1024 entries
  public void fft_1d_no_recurse(vector<fp32> real, vector<fp32> imag, sint32 log2N)
  {
    if (log2N < 0 || log2N > 31) { return; }

    sint32 N = 1 << log2N;

    # bit-reversal permutation
    # rearrange date into order expected by FFT
    loop (sint32 i = 0; i < N; i += 1)
    {
      sint32 idx = 0;
      loop (sint32 j = 0; j < log2N; j += 1)
      {
        sint32 shift = (log2N - 1 - 2 * j);
        if (shift >= 0) { idx += ((i & (1 << j)) << shift); }
        else { idx += ((i & (1 << j)) >> -shift); }
      }
      if (idx <= i) { continue; }
      fp32 swapr = real[idx];
      fp32 swapi = imag[idx];
      real[idx] = real[i];
      imag[idx] = imag[i];
      real[i] = swapr;
      imag[i] = swapi;
    }

    # FFT
    loop (sint32 Ni = 2, step = N / 2; Ni <= N; Ni *= 2, step /= 2)
    {
      fp32 scale_idx = 1024 / Ni;
      loop (sint32 off = 0; off < N; off += Ni)
      {
        loop (sint32 k = 0; k < Ni / 2; k += 1)
        {
          sint32 idx1 = k + off;
          sint32 idx2 = idx1 + Ni / 2;
          sint32 index = k * scale_idx;
          fp32 tmpr1 = real[idx1];
          fp32 tmpi1 = imag[idx1];
          fp32 tmpr2 = real[idx2];
          fp32 tmpi2 = imag[idx2];
          real[idx1] = tmpr1 + ( tmpr2 * cos_tbl[index] + tmpi2 * sin_tbl[index]);
          imag[idx1] = tmpi1 + (-tmpr2 * sin_tbl[index] + tmpi2 * cos_tbl[index]);
          real[idx2] = tmpr1 - ( tmpr2 * cos_tbl[index] + tmpi2 * sin_tbl[index]);
          imag[idx2] = tmpi1 - (-tmpr2 * sin_tbl[index] + tmpi2 * cos_tbl[index]);
        }
      }
    }
  }
}
