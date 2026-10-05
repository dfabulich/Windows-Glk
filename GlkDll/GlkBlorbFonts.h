/////////////////////////////////////////////////////////////////////////////
//
// Windows MFC Glk Libraries
//
// GlkBlorbFonts
// Fonts from the Blorb resource map
//
/////////////////////////////////////////////////////////////////////////////

#ifndef WINGLK_BLORB_FONTS_H_
#define WINGLK_BLORB_FONTS_H_

struct CWinGlkBlorbFace;
class CWinGlkBlorbFont;

namespace WinGlkBlorbFonts
{
  // Load the fonts described by the Blorb file's 'FDes' chunk, if not already done
  void Load(void);

  // True if Blorb fonts are drawn with DirectWrite, rather than registered with GDI
  bool UseDirectWrite(void);

  // Find the face of a Blorb family (given in lower case) closest to the requested
  // weight and style, following the CSS font matching rules, or NULL if there is none
  const CWinGlkBlorbFace* Match(const char* Family, int Weight, bool Italic);

  // Get the GDI family name, weight and italic flag to draw a face with GDI
  const char* GetGdiFont(const CWinGlkBlorbFace* pFace, int& Weight, bool& Italic);

  // Get a DirectWrite font for a face at the given requested weight, style and size
  CWinGlkBlorbFont* GetFont(const CWinGlkBlorbFace* pFace, int Weight, bool Italic,
    int Pixels, bool Underline);

  // Update the vertical metrics to match a DirectWrite font
  void GetMetrics(CWinGlkBlorbFont* pFont, TEXTMETRIC& Metrics);

  SIZE GetTextExtent(CWinGlkBlorbFont* pFont, LPCWSTR pText, int iCount);
  void TextOut(CWinGlkBlorbFont* pFont, HDC hdc, int x, int y, LPCWSTR pText, int iCount);
}

#endif // WINGLK_BLORB_FONTS_H_
