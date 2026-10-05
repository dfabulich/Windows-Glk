/////////////////////////////////////////////////////////////////////////////
//
// Windows MFC Glk Libraries
//
// GlkCss
// Basic profile of the CSS Glk extension
//
/////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "GlkDll.h"
#include "GlkCss.h"
#include "GlkStyle.h"
#include "GlkWindowTextBuffer.h"
#include "GlkWindowTextGrid.h"

#include <math.h>
#include <vector>

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// Parsed CSS values
/////////////////////////////////////////////////////////////////////////////

CWinGlkCssColour::CWinGlkCssColour() : m_bSet(false), m_Light(0), m_Dark(0)
{
}

bool CWinGlkCssColour::operator==(const CWinGlkCssColour& Compare) const
{
  if (m_bSet != Compare.m_bSet)
    return false;
  if (!m_bSet)
    return true;
  return (m_Light == Compare.m_Light) && (m_Dark == Compare.m_Dark);
}

CWinGlkCssLength::CWinGlkCssLength() : m_Unit(Unset), m_Value(0.0)
{
}

bool CWinGlkCssLength::operator==(const CWinGlkCssLength& Compare) const
{
  return (m_Unit == Compare.m_Unit) && (m_Value == Compare.m_Value);
}

int CWinGlkCssLength::ToPixels(double dEmPixels, int iPercentBase, int iDPI) const
{
  double px = 0.0;
  switch (m_Unit)
  {
  case Pixels:
    px = m_Value * iDPI / 96.0;
    break;
  case Em:
    px = m_Value * dEmPixels;
    break;
  case Percent:
    px = m_Value * iPercentBase / 100.0;
    break;
  }
  return (int)floor(px + 0.5);
}

CWinGlkCssFontSize::CWinGlkCssFontSize() : m_Kind(Unset), m_Value(0.0)
{
}

bool CWinGlkCssFontSize::operator==(const CWinGlkCssFontSize& Compare) const
{
  return (m_Kind == Compare.m_Kind) && (m_Value == Compare.m_Value);
}

double CWinGlkCssFontSize::Resolve(double dParentPoints, double dMediumPoints) const
{
  switch (m_Kind)
  {
  case Points:
    return m_Value;
  case ParentFactor:
    return dParentPoints * m_Value;
  case MediumFactor:
    return dMediumPoints * m_Value;
  }
  return dParentPoints;
}

/////////////////////////////////////////////////////////////////////////////
// String helpers
/////////////////////////////////////////////////////////////////////////////

std::string WinGlkCss::FromGlk(const char* pStr, glui32 iLen)
{
  if ((pStr == NULL) || (iLen == 0))
    return std::string();
  return std::string(pStr,iLen);
}

static bool IsSpace(char c)
{
  return (c == ' ') || (c == '\t') || (c == '\n') || (c == '\r') || (c == '\f');
}

std::string WinGlkCss::Trim(const std::string& str)
{
  size_t start = 0, end = str.size();
  while ((start < end) && IsSpace(str[start]))
    start++;
  while ((end > start) && IsSpace(str[end-1]))
    end--;
  return str.substr(start,end-start);
}

std::string WinGlkCss::Lower(const std::string& str)
{
  std::string result(str);
  for (size_t i = 0; i < result.size(); i++)
  {
    if ((result[i] >= 'A') && (result[i] <= 'Z'))
      result[i] = result[i] - 'A' + 'a';
  }
  return result;
}

static std::string Normalize(const std::string& str)
{
  return WinGlkCss::Lower(WinGlkCss::Trim(str));
}

static CString Utf8ToAnsi(const std::string& str)
{
  if (str.empty())
    return CString();
  int wlen = ::MultiByteToWideChar(CP_UTF8,0,str.c_str(),(int)str.size(),NULL,0);
  if (wlen <= 0)
    return CString(str.c_str());
  CStringW wide;
  ::MultiByteToWideChar(CP_UTF8,0,str.c_str(),(int)str.size(),wide.GetBuffer(wlen),wlen);
  wide.ReleaseBuffer(wlen);
  return CString(wide);
}

/////////////////////////////////////////////////////////////////////////////
// Value parsers
/////////////////////////////////////////////////////////////////////////////

namespace {

struct NamedColour
{
  const char* name;
  DWORD rgb;
};

const NamedColour NamedColours[] =
{
  { "aqua",    0x00FFFF },
  { "black",   0x000000 },
  { "blue",    0x0000FF },
  { "cyan",    0x00FFFF },
  { "fuchsia", 0xFF00FF },
  { "gray",    0x808080 },
  { "green",   0x008000 },
  { "grey",    0x808080 },
  { "lime",    0x00FF00 },
  { "magenta", 0xFF00FF },
  { "maroon",  0x800000 },
  { "navy",    0x000080 },
  { "olive",   0x808000 },
  { "orange",  0xFFA500 },
  { "pink",    0xFFC0CB },
  { "purple",  0x800080 },
  { "red",     0xFF0000 },
  { "silver",  0xC0C0C0 },
  { "teal",    0x008080 },
  { "white",   0xFFFFFF },
  { "yellow",  0xFFFF00 },
};

int HexDigit(char c)
{
  if ((c >= '0') && (c <= '9'))
    return c - '0';
  if ((c >= 'a') && (c <= 'f'))
    return c - 'a' + 10;
  return -1;
}

// Parse a single colour (not light-dark) into 0xAARRGGBB
bool ParseSimpleColour(const std::string& v, DWORD& argb)
{
  if (v == "transparent")
  {
    argb = 0;
    return true;
  }
  for (size_t i = 0; i < sizeof NamedColours / sizeof NamedColours[0]; i++)
  {
    if (v == NamedColours[i].name)
    {
      argb = 0xFF000000 | NamedColours[i].rgb;
      return true;
    }
  }
  if ((v.size() < 2) || (v[0] != '#'))
    return false;

  std::string hex = v.substr(1);
  for (size_t i = 0; i < hex.size(); i++)
  {
    if (HexDigit(hex[i]) < 0)
      return false;
  }

  int c[4] = { 0, 0, 0, 255 };
  switch (hex.size())
  {
  case 3:
  case 4:
    for (size_t i = 0; i < hex.size(); i++)
      c[i] = HexDigit(hex[i]) * 17;
    break;
  case 6:
  case 8:
    for (size_t i = 0; i < hex.size()/2; i++)
      c[i] = (HexDigit(hex[i*2]) << 4) | HexDigit(hex[i*2+1]);
    break;
  default:
    return false;
  }
  argb = ((DWORD)c[3] << 24) | ((DWORD)c[0] << 16) | ((DWORD)c[1] << 8) | (DWORD)c[2];
  return true;
}

// Split the arguments of a function at top level commas
std::vector<std::string> SplitArgs(const std::string& args)
{
  std::vector<std::string> result;
  int depth = 0;
  size_t start = 0;
  for (size_t i = 0; i < args.size(); i++)
  {
    if (args[i] == '(')
      depth++;
    else if (args[i] == ')')
      depth--;
    else if ((args[i] == ',') && (depth == 0))
    {
      result.push_back(WinGlkCss::Trim(args.substr(start,i-start)));
      start = i+1;
    }
  }
  result.push_back(WinGlkCss::Trim(args.substr(start)));
  return result;
}

bool ParseColour(const std::string& raw, CWinGlkCssColour& colour)
{
  std::string v = Normalize(raw);
  const std::string ld = "light-dark(";
  if ((v.compare(0,ld.size(),ld) == 0) && (v[v.size()-1] == ')'))
  {
    std::vector<std::string> args = SplitArgs(v.substr(ld.size(),v.size()-ld.size()-1));
    if (args.size() != 2)
      return false;
    CWinGlkCssColour light, dark;
    if (!ParseColour(args[0],light) || !ParseColour(args[1],dark))
      return false;
    colour.m_bSet = true;
    colour.m_Light = light.m_Light;
    colour.m_Dark = dark.m_Dark;
    return true;
  }

  DWORD argb;
  if (!ParseSimpleColour(v,argb))
    return false;
  colour.m_bSet = true;
  colour.m_Light = argb;
  colour.m_Dark = argb;
  return true;
}

// Parse a CSS number with a unit suffix, without depending on the C locale
bool ParseNumber(const std::string& v, double& number, std::string& unit)
{
  size_t i = 0;
  bool negative = false;
  if ((i < v.size()) && ((v[i] == '+') || (v[i] == '-')))
  {
    negative = (v[i] == '-');
    i++;
  }

  double value = 0.0;
  bool digits = false;
  while ((i < v.size()) && (v[i] >= '0') && (v[i] <= '9'))
  {
    value = (value * 10.0) + (v[i] - '0');
    digits = true;
    i++;
  }
  if ((i < v.size()) && (v[i] == '.'))
  {
    i++;
    double scale = 0.1;
    while ((i < v.size()) && (v[i] >= '0') && (v[i] <= '9'))
    {
      value += (v[i] - '0') * scale;
      scale /= 10.0;
      digits = true;
      i++;
    }
  }
  if (!digits)
    return false;

  number = negative ? -value : value;
  unit = v.substr(i);
  return true;
}

bool ParseLength(const std::string& raw, CWinGlkCssLength& length)
{
  double number;
  std::string unit;
  if (!ParseNumber(Normalize(raw),number,unit))
    return false;
  if (number < 0.0)
    return false;

  if (unit.empty())
  {
    if (number != 0.0)
      return false;
    length.m_Unit = CWinGlkCssLength::Pixels;
    length.m_Value = 0.0;
  }
  else if (unit == "px")
  {
    length.m_Unit = CWinGlkCssLength::Pixels;
    length.m_Value = number;
  }
  else if (unit == "pt")
  {
    length.m_Unit = CWinGlkCssLength::Pixels;
    length.m_Value = number * 96.0 / 72.0;
  }
  else if (unit == "em")
  {
    length.m_Unit = CWinGlkCssLength::Em;
    length.m_Value = number;
  }
  else if (unit == "%")
  {
    length.m_Unit = CWinGlkCssLength::Percent;
    length.m_Value = number;
  }
  else
    return false;
  return true;
}

bool ParseFontSize(const std::string& raw, CWinGlkCssFontSize& size)
{
  std::string v = Normalize(raw);
  if (v == "small")
  {
    size.m_Kind = CWinGlkCssFontSize::MediumFactor;
    size.m_Value = 13.0 / 16.0;
    return true;
  }
  if (v == "medium")
  {
    size.m_Kind = CWinGlkCssFontSize::MediumFactor;
    size.m_Value = 1.0;
    return true;
  }
  if (v == "large")
  {
    size.m_Kind = CWinGlkCssFontSize::MediumFactor;
    size.m_Value = 18.0 / 16.0;
    return true;
  }
  if (v == "larger")
  {
    size.m_Kind = CWinGlkCssFontSize::ParentFactor;
    size.m_Value = 1.2;
    return true;
  }
  if (v == "smaller")
  {
    size.m_Kind = CWinGlkCssFontSize::ParentFactor;
    size.m_Value = 1.0 / 1.2;
    return true;
  }

  CWinGlkCssLength length;
  if (!ParseLength(v,length))
    return false;
  switch (length.m_Unit)
  {
  case CWinGlkCssLength::Pixels:
    size.m_Kind = CWinGlkCssFontSize::Points;
    size.m_Value = length.m_Value * 72.0 / 96.0;
    break;
  case CWinGlkCssLength::Em:
    size.m_Kind = CWinGlkCssFontSize::ParentFactor;
    size.m_Value = length.m_Value;
    break;
  case CWinGlkCssLength::Percent:
    size.m_Kind = CWinGlkCssFontSize::ParentFactor;
    size.m_Value = length.m_Value / 100.0;
    break;
  default:
    return false;
  }
  return true;
}

int ParseKeyword(const std::string& raw, const char* on1, const char* on2,
  const char* off1, const char* off2)
{
  std::string v = Normalize(raw);
  if ((on1 && (v == on1)) || (on2 && (v == on2)))
    return 1;
  if ((off1 && (v == off1)) || (off2 && (v == off2)))
    return 0;
  return -1;
}

int ParseJustify(const std::string& raw)
{
  std::string v = Normalize(raw);
  if ((v == "left") || (v == "start"))
    return stylehint_just_LeftFlush;
  if ((v == "right") || (v == "end"))
    return stylehint_just_RightFlush;
  if (v == "center")
    return stylehint_just_Centered;
  if (v == "justify")
    return stylehint_just_LeftRight;
  return -1;
}

} // namespace

/////////////////////////////////////////////////////////////////////////////
// CWinGlkCssAttrs
/////////////////////////////////////////////////////////////////////////////

CWinGlkCssAttrs::CWinGlkCssAttrs() : m_Reverse(-1), m_Bold(-1), m_Italic(-1),
  m_Underline(-1), m_SpanBorder(-1), m_ParaBorder(-1), m_Justify(-1)
{
}

bool CWinGlkCssAttrs::operator==(const CWinGlkCssAttrs& Compare) const
{
  return (m_Fore == Compare.m_Fore) && (m_Back == Compare.m_Back) &&
    (m_Reverse == Compare.m_Reverse) && (m_Bold == Compare.m_Bold) &&
    (m_Italic == Compare.m_Italic) && (m_Underline == Compare.m_Underline) &&
    (m_SpanBorder == Compare.m_SpanBorder) && (m_Size == Compare.m_Size) &&
    (m_Family == Compare.m_Family) && (m_ParaBack == Compare.m_ParaBack) &&
    (m_ParaBorder == Compare.m_ParaBorder) && (m_Justify == Compare.m_Justify) &&
    (m_MarginLeft == Compare.m_MarginLeft) && (m_MarginRight == Compare.m_MarginRight) &&
    (m_TextIndent == Compare.m_TextIndent);
}

void CWinGlkCssAttrs::Parse(const CWinGlkCssProps& Props, bool bParagraph)
{
  for (CWinGlkCssProps::const_iterator it = Props.begin(); it != Props.end(); ++it)
  {
    const std::string& prop = it->first;
    const std::string& val = it->second;
    int i;

    if (prop == "color")
      ParseColour(val,m_Fore);
    else if (prop == "background-color")
      ParseColour(val,bParagraph ? m_ParaBack : m_Back);
    else if (prop == "-iftf-reverse-video")
    {
      if ((i = ParseKeyword(val,"reverse",NULL,"none",NULL)) >= 0)
        m_Reverse = i;
    }
    else if (prop == "font-weight")
    {
      if ((i = ParseKeyword(val,"bold","700","normal","400")) >= 0)
        m_Bold = i;
    }
    else if (prop == "font-style")
    {
      if ((i = ParseKeyword(val,"italic","oblique","normal",NULL)) >= 0)
        m_Italic = i;
    }
    else if (prop == "font-size")
      ParseFontSize(val,m_Size);
    else if (prop == "font-family")
    {
      std::string family = WinGlkCss::Trim(val);
      if (!family.empty())
        m_Family = Utf8ToAnsi(family);
    }
    else if ((prop == "text-decoration") || (prop == "text-decoration-line"))
    {
      if ((i = ParseKeyword(val,"underline",NULL,"none",NULL)) >= 0)
        m_Underline = i;
    }
    else if (prop == "text-align")
    {
      if ((i = ParseJustify(val)) >= 0)
        m_Justify = i;
    }
    else if (prop == "margin-left")
      ParseLength(val,m_MarginLeft);
    else if (prop == "margin-right")
      ParseLength(val,m_MarginRight);
    else if (prop == "text-indent")
      ParseLength(val,m_TextIndent);
    else if (prop == "border-style")
    {
      if ((i = ParseKeyword(val,"solid",NULL,"none",NULL)) >= 0)
      {
        if (bParagraph)
          m_ParaBorder = i;
        else
          m_SpanBorder = i;
      }
    }
  }
}

void CWinGlkCssAttrs::Overlay(const CWinGlkCssAttrs& Top)
{
  if (Top.m_Fore.IsSet())
    m_Fore = Top.m_Fore;
  if (Top.m_Back.IsSet())
    m_Back = Top.m_Back;
  if (Top.m_Reverse >= 0)
    m_Reverse = Top.m_Reverse;
  if (Top.m_Bold >= 0)
    m_Bold = Top.m_Bold;
  if (Top.m_Italic >= 0)
    m_Italic = Top.m_Italic;
  if (Top.m_Underline >= 0)
    m_Underline = Top.m_Underline;
  if (Top.m_SpanBorder >= 0)
    m_SpanBorder = Top.m_SpanBorder;
  if (Top.m_Size.IsSet())
    m_Size = Top.m_Size;
  if (!Top.m_Family.IsEmpty())
    m_Family = Top.m_Family;
  if (Top.m_ParaBack.IsSet())
    m_ParaBack = Top.m_ParaBack;
  if (Top.m_ParaBorder >= 0)
    m_ParaBorder = Top.m_ParaBorder;
  if (Top.m_Justify >= 0)
    m_Justify = Top.m_Justify;
  if (Top.m_MarginLeft.IsSet())
    m_MarginLeft = Top.m_MarginLeft;
  if (Top.m_MarginRight.IsSet())
    m_MarginRight = Top.m_MarginRight;
  if (Top.m_TextIndent.IsSet())
    m_TextIndent = Top.m_TextIndent;
}

void CWinGlkCssAttrs::InheritFrom(const CWinGlkCssAttrs& Parent)
{
  if (!m_Fore.IsSet())
    m_Fore = Parent.m_Fore;
  if (m_Bold < 0)
    m_Bold = Parent.m_Bold;
  if (m_Italic < 0)
    m_Italic = Parent.m_Italic;
  if (m_Underline < 0)
    m_Underline = Parent.m_Underline;
  if (!m_Size.IsSet())
    m_Size = Parent.m_Size;
  if (m_Family.IsEmpty())
    m_Family = Parent.m_Family;
  if (m_Justify < 0)
    m_Justify = Parent.m_Justify;
  if (!m_TextIndent.IsSet())
    m_TextIndent = Parent.m_TextIndent;
}

bool CWinGlkCssAttrs::IsEmpty(void) const
{
  static const CWinGlkCssAttrs Empty;
  return *this == Empty;
}

CWinGlkCssAttrs* CWinGlkCssAttrs::CopyOrNull(void) const
{
  if (IsEmpty())
    return NULL;
  return new CWinGlkCssAttrs(*this);
}

CWinGlkCssWindowHints::CWinGlkCssWindowHints() : m_bBorder(false)
{
}

/////////////////////////////////////////////////////////////////////////////
// Global CSS hint storage
/////////////////////////////////////////////////////////////////////////////

namespace {

struct CssHintTable
{
  CWinGlkCssProps m_Styles[style_NUMSTYLES][2];
  CWinGlkCssProps m_Hyperlinks[style_NUMSTYLES];
  CWinGlkCssProps m_Window;
  CWinGlkCssProps m_Input;
  CWinGlkCssProps m_Image;
};

CssHintTable BufferHints;
CssHintTable GridHints;

CssHintTable* GetTable(glui32 wintype)
{
  switch (wintype)
  {
  case wintype_TextBuffer:
    return &BufferHints;
  case wintype_TextGrid:
    return &GridHints;
  }
  return NULL;
}

CWinGlkStyles* GetDefaultStyles(glui32 wintype)
{
  switch (wintype)
  {
  case wintype_TextBuffer:
    return CWinGlkWndTextBuffer::GetDefaultStyles();
  case wintype_TextGrid:
    return CWinGlkWndTextGrid::GetDefaultStyles();
  }
  return NULL;
}

void UpdateStyle(glui32 wintype, glui32 style)
{
  CssHintTable* pTable = GetTable(wintype);
  CWinGlkStyles* pStyles = GetDefaultStyles(wintype);
  if ((pTable == NULL) || (pStyles == NULL) || (style >= style_NUMSTYLES))
    return;

  CWinGlkCssAttrs Attrs;
  Attrs.Parse(pTable->m_Styles[style][CSS_Span],false);
  Attrs.Parse(pTable->m_Styles[style][CSS_Paragraph],true);
  pStyles->GetStyle(style)->m_Css = Attrs;
}

void SetProp(CWinGlkCssProps& Props, const std::string& prop, const std::string* pVal)
{
  if (pVal)
    Props[prop] = *pVal;
  else
    Props.erase(prop);
}

} // namespace

void WinGlkCss::HintSet(glui32 wintype, glui32 target, glui32 style,
  const std::string& prop, const std::string* pVal)
{
  if (wintype == wintype_AllTypes)
  {
    HintSet(wintype_TextBuffer,target,style,prop,pVal);
    HintSet(wintype_TextGrid,target,style,prop,pVal);
    return;
  }

  CssHintTable* pTable = GetTable(wintype);
  if (pTable == NULL)
    return;

  switch (target)
  {
  case CSS_Span:
  case CSS_Paragraph:
    if (style < style_NUMSTYLES)
    {
      SetProp(pTable->m_Styles[style][target],prop,pVal);
      UpdateStyle(wintype,style);
    }
    break;
  case CSS_Hyperlink:
    if (style < style_NUMSTYLES)
      SetProp(pTable->m_Hyperlinks[style],prop,pVal);
    break;
  case CSS_Image:
    SetProp(pTable->m_Image,prop,pVal);
    break;
  case CSS_Input:
    SetProp(pTable->m_Input,prop,pVal);
    break;
  case CSS_Window:
    SetProp(pTable->m_Window,prop,pVal);
    break;
  }
}

void WinGlkCss::HintClearAllByStyle(glui32 wintype, glui32 style)
{
  if (wintype == wintype_AllTypes)
  {
    HintClearAllByStyle(wintype_TextBuffer,style);
    HintClearAllByStyle(wintype_TextGrid,style);
    return;
  }

  CssHintTable* pTable = GetTable(wintype);
  if ((pTable == NULL) || (style >= style_NUMSTYLES))
    return;

  pTable->m_Styles[style][CSS_Span].clear();
  pTable->m_Styles[style][CSS_Paragraph].clear();
  pTable->m_Hyperlinks[style].clear();
  UpdateStyle(wintype,style);

  for (int hint = 0; hint < stylehint_NUMHINTS; hint++)
  {
    if (wintype == wintype_TextBuffer)
      CWinGlkWndTextBuffer::ClearStyleHint(style,hint);
    else
      CWinGlkWndTextGrid::ClearStyleHint(style,hint);
  }
}

void WinGlkCss::HintClearAllByWindow(glui32 wintype)
{
  if (wintype == wintype_AllTypes)
  {
    HintClearAllByWindow(wintype_TextBuffer);
    HintClearAllByWindow(wintype_TextGrid);
    return;
  }

  CssHintTable* pTable = GetTable(wintype);
  if (pTable == NULL)
    return;

  for (glui32 style = 0; style < style_NUMSTYLES; style++)
    HintClearAllByStyle(wintype,style);
  pTable->m_Window.clear();
  pTable->m_Input.clear();
  pTable->m_Image.clear();
}

void WinGlkCss::SnapshotWindow(glui32 wintype, CWinGlkStyles& Styles, CWinGlkCssWindowHints& Hints)
{
  CssHintTable* pTable = GetTable(wintype);
  if (pTable == NULL)
    return;

  for (int i = 0; i < style_NUMSTYLES; i++)
  {
    Hints.m_Hyperlinks[i] = CWinGlkCssAttrs();
    Hints.m_Hyperlinks[i].Parse(pTable->m_Hyperlinks[i],false);
  }
  Hints.m_Input = CWinGlkCssAttrs();
  Hints.m_Input.Parse(pTable->m_Input,false);
  Hints.m_Image = CWinGlkCssAttrs();
  Hints.m_Image.Parse(pTable->m_Image,false);

  CWinGlkCssAttrs Window;
  Window.Parse(pTable->m_Window,false);
  Hints.m_Back = Window.m_Back;
  Hints.m_bBorder = (Window.m_SpanBorder == 1);

  // Inheritable window properties cascade into styles that do not set them,
  // but a stylehint text colour takes priority over the window's colour
  for (int i = 0; i < style_NUMSTYLES; i++)
  {
    CWinGlkStyle* pStyle = Styles.GetStyle(i);
    CWinGlkCssAttrs Parent(Window);
    if (pStyle->m_TextColour != WINGLK_COLOUR_TEXT)
      Parent.m_Fore = CWinGlkCssColour();
    pStyle->m_Css.InheritFrom(Parent);
  }
}

/////////////////////////////////////////////////////////////////////////////
// CSS feature detection
/////////////////////////////////////////////////////////////////////////////

bool WinGlkCss::Supports(const std::string& rawProp, const std::string& rawVal)
{
  std::string prop = Normalize(rawProp);
  std::string val = Trim(rawVal);

  static const char* Known[] =
  {
    "color", "background-color", "-iftf-reverse-video", "font-weight", "font-style",
    "font-size", "font-family", "text-decoration", "text-decoration-line",
    "text-align", "margin-left", "margin-right", "text-indent", "border-style"
  };
  bool known = false;
  for (size_t i = 0; i < sizeof Known / sizeof Known[0]; i++)
  {
    if (prop == Known[i])
      known = true;
  }
  if (!known)
    return false;
  if (val.empty())
    return true;

  CWinGlkCssColour colour;
  CWinGlkCssLength length;
  CWinGlkCssFontSize size;
  if ((prop == "color") || (prop == "background-color"))
    return ParseColour(val,colour);
  if (prop == "-iftf-reverse-video")
    return ParseKeyword(val,"reverse",NULL,"none",NULL) >= 0;
  if (prop == "font-weight")
    return ParseKeyword(val,"bold","700","normal","400") >= 0;
  if (prop == "font-style")
    return ParseKeyword(val,"italic","oblique","normal",NULL) >= 0;
  if (prop == "font-size")
    return ParseFontSize(val,size);
  if (prop == "font-family")
    return true;
  if ((prop == "text-decoration") || (prop == "text-decoration-line"))
    return ParseKeyword(val,"underline",NULL,"none",NULL) >= 0;
  if (prop == "text-align")
    return ParseJustify(val) >= 0;
  if ((prop == "margin-left") || (prop == "margin-right") || (prop == "text-indent"))
    return ParseLength(val,length);
  if (prop == "border-style")
    return ParseKeyword(val,"solid",NULL,"none",NULL) >= 0;
  return false;
}

/////////////////////////////////////////////////////////////////////////////
// Font family resolution
/////////////////////////////////////////////////////////////////////////////

namespace {

struct InstalledFont
{
  InstalledFont() : m_bFound(false), m_PitchAndFamily(0) {}

  bool m_bFound;
  BYTE m_PitchAndFamily;
  CString m_Face;
};

int CALLBACK EnumFontProc(const LOGFONT* pLogFont, const TEXTMETRIC*, DWORD dwType, LPARAM lParam)
{
  if (dwType & RASTER_FONTTYPE)
    return 1;
  InstalledFont* pFont = (InstalledFont*)lParam;
  pFont->m_bFound = true;
  pFont->m_PitchAndFamily = pLogFont->lfPitchAndFamily;
  pFont->m_Face = pLogFont->lfFaceName;
  return 0;
}

const InstalledFont& LookupFont(const CString& Name)
{
  static std::map<CString,InstalledFont> Cache;

  CString Key(Name);
  Key.MakeLower();
  std::map<CString,InstalledFont>::const_iterator it = Cache.find(Key);
  if (it != Cache.end())
    return it->second;

  InstalledFont Font;
  if ((Name.GetLength() > 0) && (Name.GetLength() < LF_FACESIZE))
  {
    LOGFONT LogFont;
    ::ZeroMemory(&LogFont,sizeof LogFont);
    LogFont.lfCharSet = DEFAULT_CHARSET;
    lstrcpyn(LogFont.lfFaceName,Name,LF_FACESIZE);

    HDC hdc = ::GetDC(NULL);
    ::EnumFontFamiliesEx(hdc,&LogFont,(FONTENUMPROC)EnumFontProc,(LPARAM)&Font,0);
    ::ReleaseDC(NULL,hdc);
  }
  return Cache[Key] = Font;
}

bool FirstInstalled(const char* const* Names, int iCount, CString& Face)
{
  for (int i = 0; i < iCount; i++)
  {
    const InstalledFont& Font = LookupFont(Names[i]);
    if (Font.m_bFound)
    {
      Face = Font.m_Face;
      return true;
    }
  }
  return false;
}

} // namespace

bool WinGlkCss::ResolveFamily(const CString& Family, CString& Face, bool& bMonospace)
{
  CGlkApp* pApp = (CGlkApp*)AfxGetApp();

  int iPos = 0;
  while (iPos >= 0)
  {
    CString Name = Family.Tokenize(",",iPos);
    if (iPos < 0)
      break;
    Name.Trim();
    if ((Name.GetLength() >= 2) &&
      ((Name[0] == '"') || (Name[0] == '\'')) && (Name[Name.GetLength()-1] == Name[0]))
    {
      Name = Name.Mid(1,Name.GetLength()-2);
      Name.Trim();
    }
    if (Name.IsEmpty())
      continue;

    CString Lower(Name);
    Lower.MakeLower();

    if (Lower == "monospace")
    {
      Face = pApp->GetFixedFontName();
      bMonospace = true;
      return true;
    }
    if (Lower == "serif")
    {
      static const char* Serifs[] = { "Times New Roman", "Georgia", "Cambria" };
      const InstalledFont& Prop = LookupFont(pApp->GetPropFontName());
      if (Prop.m_bFound && ((Prop.m_PitchAndFamily & 0xF0) == FF_ROMAN))
        Face = pApp->GetPropFontName();
      else if (!FirstInstalled(Serifs,sizeof Serifs / sizeof Serifs[0],Face))
        Face = pApp->GetPropFontName();
      bMonospace = false;
      return true;
    }
    if (Lower == "sans-serif")
    {
      static const char* Sans[] = { "Segoe UI", "Arial", "Tahoma" };
      const InstalledFont& Prop = LookupFont(pApp->GetPropFontName());
      if (Prop.m_bFound && ((Prop.m_PitchAndFamily & 0xF0) == FF_SWISS))
        Face = pApp->GetPropFontName();
      else if (!FirstInstalled(Sans,sizeof Sans / sizeof Sans[0],Face))
        Face = pApp->GetPropFontName();
      bMonospace = false;
      return true;
    }

    const InstalledFont* pFont = &LookupFont(Name);
    if (!pFont->m_bFound)
    {
      if (Lower == "courier")
        pFont = &LookupFont("Courier New");
      else if (Lower == "helvetica")
        pFont = &LookupFont("Arial");
      else if (Lower == "times")
        pFont = &LookupFont("Times New Roman");
    }
    if (pFont->m_bFound)
    {
      Face = pFont->m_Face;
      bMonospace = ((pFont->m_PitchAndFamily & 0x03) == FIXED_PITCH);
      return true;
    }
  }
  return false;
}

/////////////////////////////////////////////////////////////////////////////
// Drawing helpers
/////////////////////////////////////////////////////////////////////////////

COLORREF WinGlkCss::Blend(DWORD Colour, COLORREF Under)
{
  int a = (Colour >> 24) & 0xFF;
  int r = (Colour >> 16) & 0xFF;
  int g = (Colour >> 8) & 0xFF;
  int b = Colour & 0xFF;
  if (a == 255)
    return RGB(r,g,b);
  if (a == 0)
    return Under;
  r = (r * a + GetRValue(Under) * (255 - a)) / 255;
  g = (g * a + GetGValue(Under) * (255 - a)) / 255;
  b = (b * a + GetBValue(Under) * (255 - a)) / 255;
  return RGB(r,g,b);
}

void WinGlkCss::FrameRect(CDC& dc, const CRect& Rect, COLORREF Colour, int iDPI)
{
  if ((Rect.Width() <= 0) || (Rect.Height() <= 0))
    return;

  int w = max(1,MulDiv(1,iDPI,96));
  COLORREF OldBack = dc.GetBkColor();
  dc.FillSolidRect(Rect.left,Rect.top,Rect.Width(),w,Colour);
  dc.FillSolidRect(Rect.left,Rect.bottom-w,Rect.Width(),w,Colour);
  dc.FillSolidRect(Rect.left,Rect.top,w,Rect.Height(),Colour);
  dc.FillSolidRect(Rect.right-w,Rect.top,w,Rect.Height(),Colour);
  dc.SetBkColor(OldBack);
}
