/////////////////////////////////////////////////////////////////////////////
//
// Windows MFC Glk Libraries
//
// GlkCss
// Basic profile of the CSS Glk extension
//
/////////////////////////////////////////////////////////////////////////////

#ifndef WINGLK_CSS_H_
#define WINGLK_CSS_H_

extern "C"
{
#include "glk.h"
}
#include "WinGlk.h"

#include <map>
#include <string>

/////////////////////////////////////////////////////////////////////////////
// CSS property storage
/////////////////////////////////////////////////////////////////////////////

// Property names are lower case, values are trimmed UTF-8
typedef std::map<std::string,std::string> CWinGlkCssProps;

/////////////////////////////////////////////////////////////////////////////
// Parsed CSS values
/////////////////////////////////////////////////////////////////////////////

struct CWinGlkCssColour
{
  CWinGlkCssColour();
  bool operator==(const CWinGlkCssColour& Compare) const;

  bool IsSet(void) const { return m_bSet; }
  DWORD Get(bool bDark) const { return bDark ? m_Dark : m_Light; }

  // Colours are 0xAARRGGBB, with separate values for light-dark()
  bool m_bSet;
  DWORD m_Light;
  DWORD m_Dark;
};

struct CWinGlkCssLength
{
  enum Unit { Unset, Pixels, Em, Percent };

  CWinGlkCssLength();
  bool operator==(const CWinGlkCssLength& Compare) const;

  bool IsSet(void) const { return m_Unit != Unset; }
  int ToPixels(double dEmPixels, int iPercentBase, int iDPI) const;

  Unit m_Unit;
  double m_Value;
};

struct CWinGlkCssFontSize
{
  enum Kind { Unset, Points, ParentFactor, MediumFactor };

  CWinGlkCssFontSize();
  bool operator==(const CWinGlkCssFontSize& Compare) const;

  bool IsSet(void) const { return m_Kind != Unset; }
  double Resolve(double dParentPoints, double dMediumPoints) const;

  Kind m_Kind;
  double m_Value;
};

// The result of parsing a set of CSS properties. Integer fields are -1 if unset.
struct CWinGlkCssAttrs
{
  CWinGlkCssAttrs();
  bool operator==(const CWinGlkCssAttrs& Compare) const;
  bool operator!=(const CWinGlkCssAttrs& Compare) const { return !(*this == Compare); }

  void Parse(const CWinGlkCssProps& Props, bool bParagraph);
  void Overlay(const CWinGlkCssAttrs& Top);
  void InheritFrom(const CWinGlkCssAttrs& Parent);
  bool IsEmpty(void) const;
  CWinGlkCssAttrs* CopyOrNull(void) const;

  // Span level
  CWinGlkCssColour m_Fore;
  CWinGlkCssColour m_Back;
  int m_Reverse;
  int m_Bold;
  int m_Italic;
  int m_Underline;
  int m_SpanBorder;
  CWinGlkCssFontSize m_Size;
  CString m_Family;

  // Paragraph level
  CWinGlkCssColour m_ParaBack;
  int m_ParaBorder;
  int m_Justify;
  CWinGlkCssLength m_MarginLeft;
  CWinGlkCssLength m_MarginRight;
  CWinGlkCssLength m_TextIndent;
};

/////////////////////////////////////////////////////////////////////////////
// CSS hints captured when a window is opened
/////////////////////////////////////////////////////////////////////////////

class CWinGlkStyles;

struct CWinGlkCssWindowHints
{
  CWinGlkCssWindowHints();

  CWinGlkCssAttrs m_Hyperlinks[style_NUMSTYLES];
  CWinGlkCssAttrs m_Input;
  CWinGlkCssAttrs m_Image;
  CWinGlkCssColour m_Back;
  bool m_bBorder;
};

/////////////////////////////////////////////////////////////////////////////
// CSS functions
/////////////////////////////////////////////////////////////////////////////

namespace WinGlkCss
{
  std::string FromGlk(const char* pStr, glui32 iLen);
  std::string Trim(const std::string& str);
  std::string Lower(const std::string& str);

  void HintSet(glui32 wintype, glui32 target, glui32 style,
    const std::string& prop, const std::string* pVal);
  void HintClearAllByStyle(glui32 wintype, glui32 style);
  void HintClearAllByWindow(glui32 wintype);

  // Apply the current CSS window hints to a newly opened window
  void SnapshotWindow(glui32 wintype, CWinGlkStyles& Styles, CWinGlkCssWindowHints& Hints);

  bool Supports(const std::string& prop, const std::string& val);

  // Map a CSS font-family list onto an installed font
  bool ResolveFamily(const CString& Family, CString& Face, bool& bMonospace);

  // Composite a CSS colour onto an opaque background
  COLORREF Blend(DWORD Colour, COLORREF Under);

  // Draw a one pixel (scaled for DPI) border inside the rectangle
  void FrameRect(CDC& dc, const CRect& Rect, COLORREF Colour, int iDPI);
}

#endif // WINGLK_CSS_H_
