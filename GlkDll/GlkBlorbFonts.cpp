/////////////////////////////////////////////////////////////////////////////
//
// Windows MFC Glk Libraries
//
// GlkBlorbFonts
// Fonts from the Blorb resource map
//
/////////////////////////////////////////////////////////////////////////////

// This file does not use the precompiled header, as the DirectWrite interfaces
// for variable fonts need a later Windows version than the rest of the library.
#define WINVER 0x0A00
#define _WIN32_WINNT 0x0A00
#define NTDDI_VERSION 0x0A000005
#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <d2d1.h>
#include <dwrite_3.h>
#include <atlbase.h>

#include <math.h>
#include <map>
#include <string>
#include <vector>

extern "C"
{
#include "glk.h"
#include "gi_blorb.h"
}

#include "GlkBlorbFonts.h"

// A face of a family declared in the Blorb 'FDes' chunk
struct CWinGlkBlorbFace
{
  std::string m_Family; // FDes family name, lower case
  int m_Weight;         // FDes weight, 1 to 1000
  int m_Style;          // FDes style: 0 normal, 1 italic, 2 oblique
  size_t m_Resource;    // Index into the loaded font resources

  // For DirectWrite, a collection holding just the instance of the font for this face
  IDWriteFontCollection* m_pCollection;
  std::wstring m_DWriteFamily;
};

// A DirectWrite font at a particular size
class CWinGlkBlorbFont
{
public:
  CWinGlkBlorbFont() : m_pFormat(NULL), m_bUnderline(false), m_iAscent(0), m_iDescent(0),
    m_iLineGap(0), m_iPixels(0) {}

  bool CreateLayout(LPCWSTR pText, int iCount, IDWriteTextLayout** ppLayout);
  float GetWidth(LPCWSTR pText, int iCount);

  IDWriteTextFormat* m_pFormat;
  bool m_bUnderline;
  int m_iAscent;
  int m_iDescent;
  int m_iLineGap;
  int m_iPixels;
  std::map<std::wstring,float> m_Widths;
};

namespace {

// A font resource loaded from the Blorb file
struct BlorbResource
{
  BlorbResource() : m_GdiWeight(FW_NORMAL), m_bGdiItalic(false), m_bVariable(false),
    m_pResource(NULL) {}

  // Details for drawing with GDI
  std::string m_GdiFamily;
  int m_GdiWeight;
  bool m_bGdiItalic;
  bool m_bVariable;

  // Details for drawing with DirectWrite
  IDWriteFontResource* m_pResource;
  std::vector<DWRITE_FONT_AXIS_RANGE> m_Axes;
};

std::vector<CWinGlkBlorbFace> Faces;
std::vector<BlorbResource> Resources;
std::map<std::string,CWinGlkBlorbFont*> Fonts;

// DirectWrite and Direct2D objects, which live until the process exits
IDWriteFactory6* DWriteFactory = NULL;
IDWriteInMemoryFontFileLoader* MemoryLoader = NULL;
ID2D1Factory* D2DFactory = NULL;
ID2D1DCRenderTarget* RenderTarget = NULL;

const WCHAR* Locale = L"en-us";

DWORD ReadBE32(const BYTE* p)
{
  return ((DWORD)p[0]<<24)|((DWORD)p[1]<<16)|((DWORD)p[2]<<8)|p[3];
}

WORD ReadBE16(const BYTE* p)
{
  return (WORD)((p[0]<<8)|p[1]);
}

std::string Utf8ToLowerAnsi(const char* pText, int iLength)
{
  std::string Result;
  int iWide = ::MultiByteToWideChar(CP_UTF8,0,pText,iLength,NULL,0);
  if (iWide > 0)
  {
    std::vector<WCHAR> Wide(iWide);
    ::MultiByteToWideChar(CP_UTF8,0,pText,iLength,&Wide[0],iWide);
    int iAnsi = ::WideCharToMultiByte(CP_ACP,0,&Wide[0],iWide,NULL,0,NULL,NULL);
    if (iAnsi > 0)
    {
      Result.resize(iAnsi);
      ::WideCharToMultiByte(CP_ACP,0,&Wide[0],iWide,&Result[0],iAnsi,NULL,NULL);
      ::CharLowerBuffA(&Result[0],iAnsi);
    }
  }
  return Result;
}

// Check that this is a single TrueType font, and get its Windows family name, weight and style
bool ReadTrueType(const BYTE* pData, DWORD iLength, BlorbResource& Res)
{
  if (iLength < 12)
    return false;
  DWORD version = ReadBE32(pData);
  if ((version != 0x00010000) && (version != 0x74727565)) // Not a collection or CFF font
    return false;

  const BYTE* pName = NULL;
  DWORD iNameLen = 0;
  const BYTE* pOS2 = NULL;
  DWORD iOS2Len = 0;
  bool bGlyf = false;

  DWORD iTables = ReadBE16(pData+4);
  if (12 + (iTables*16) > iLength)
    return false;
  for (DWORD i = 0; i < iTables; i++)
  {
    const BYTE* pRec = pData + 12 + (i*16);
    DWORD tag = ReadBE32(pRec);
    DWORD offset = ReadBE32(pRec+8);
    DWORD length = ReadBE32(pRec+12);
    if ((offset > iLength) || (length > iLength - offset))
      return false;
    switch (tag)
    {
    case 0x6E616D65: // 'name'
      pName = pData + offset;
      iNameLen = length;
      break;
    case 0x4F532F32: // 'OS/2'
      pOS2 = pData + offset;
      iOS2Len = length;
      break;
    case 0x676C7966: // 'glyf'
      bGlyf = true;
      break;
    case 0x66766172: // 'fvar'
      Res.m_bVariable = true;
      break;
    }
  }
  if (!bGlyf || (pName == NULL) || (iNameLen < 6))
    return false;

  if (iOS2Len >= 6)
    Res.m_GdiWeight = ReadBE16(pOS2+4);
  if (iOS2Len >= 64)
    Res.m_bGdiItalic = (ReadBE16(pOS2+62) & 1) != 0;

  // Find the Windows platform font family name, preferring US English
  DWORD iCount = ReadBE16(pName+2);
  DWORD iStrings = ReadBE16(pName+4);
  int iBestScore = 0;
  for (DWORD i = 0; i < iCount; i++)
  {
    if (6 + ((i+1)*12) > iNameLen)
      break;
    const BYTE* pRec = pName + 6 + (i*12);
    WORD platform = ReadBE16(pRec);
    WORD encoding = ReadBE16(pRec+2);
    WORD language = ReadBE16(pRec+4);
    WORD nameId = ReadBE16(pRec+6);
    DWORD length = ReadBE16(pRec+8);
    DWORD offset = iStrings + ReadBE16(pRec+10);
    if ((platform != 3) || ((encoding != 0) && (encoding != 1)) || (nameId != 1))
      continue;
    if ((offset > iNameLen) || (length > iNameLen - offset))
      continue;
    int iScore = (language == 0x409) ? 2 : 1;
    if (iScore <= iBestScore)
      continue;

    std::vector<WCHAR> Wide(length/2);
    for (size_t j = 0; j < Wide.size(); j++)
      Wide[j] = ReadBE16(pName+offset+(j*2));
    if (Wide.empty())
      continue;
    int iAnsi = ::WideCharToMultiByte(CP_ACP,0,&Wide[0],(int)Wide.size(),NULL,0,NULL,NULL);
    if (iAnsi <= 0)
      continue;
    Res.m_GdiFamily.resize(iAnsi);
    ::WideCharToMultiByte(CP_ACP,0,&Wide[0],(int)Wide.size(),&Res.m_GdiFamily[0],iAnsi,NULL,NULL);
    iBestScore = iScore;
  }
  return !Res.m_GdiFamily.empty() && (Res.m_GdiFamily.size() < LF_FACESIZE);
}

typedef HRESULT (WINAPI *DWRITECREATEFACTORY)(DWRITE_FACTORY_TYPE, REFIID, IUnknown**);
typedef HRESULT (WINAPI *D2D1CREATEFACTORY)(D2D1_FACTORY_TYPE, REFIID,
  const D2D1_FACTORY_OPTIONS*, void**);

// Set up DirectWrite, if the version supporting variable fonts is available
bool InitDirectWrite(void)
{
  static bool bInit = false;
  if (bInit)
    return (RenderTarget != NULL);
  bInit = true;

  HMODULE hDWrite = ::LoadLibraryA("dwrite.dll");
  HMODULE hD2D = ::LoadLibraryA("d2d1.dll");
  if ((hDWrite == 0) || (hD2D == 0))
    return false;
  DWRITECREATEFACTORY pDWriteCreateFactory =
    (DWRITECREATEFACTORY)::GetProcAddress(hDWrite,"DWriteCreateFactory");
  D2D1CREATEFACTORY pD2D1CreateFactory =
    (D2D1CREATEFACTORY)::GetProcAddress(hD2D,"D2D1CreateFactory");
  if ((pDWriteCreateFactory == NULL) || (pD2D1CreateFactory == NULL))
    return false;

  IUnknown* pUnknown = NULL;
  if (FAILED((*pDWriteCreateFactory)(DWRITE_FACTORY_TYPE_SHARED,__uuidof(IDWriteFactory6),&pUnknown)))
    return false;
  DWriteFactory = (IDWriteFactory6*)pUnknown;
  if (FAILED(DWriteFactory->CreateInMemoryFontFileLoader(&MemoryLoader)))
    return false;
  if (FAILED(DWriteFactory->RegisterFontFileLoader(MemoryLoader)))
    return false;

  if (FAILED((*pD2D1CreateFactory)(D2D1_FACTORY_TYPE_SINGLE_THREADED,__uuidof(ID2D1Factory),
    NULL,(void**)&D2DFactory)))
  {
    return false;
  }
  D2D1_RENDER_TARGET_PROPERTIES Props = D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_DEFAULT,
    D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,D2D1_ALPHA_MODE_IGNORE),96.0f,96.0f);
  if (FAILED(D2DFactory->CreateDCRenderTarget(&Props,&RenderTarget)))
  {
    RenderTarget = NULL;
    return false;
  }
  RenderTarget->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_CLEARTYPE);
  return true;
}

// Load the font into DirectWrite, and get the ranges of its variation axes
bool LoadDirectWrite(const void* pData, UINT32 iLength, BlorbResource& Res)
{
  CComPtr<IDWriteFontFile> File;
  if (FAILED(MemoryLoader->CreateInMemoryFontFileReference(DWriteFactory,pData,iLength,NULL,&File)))
    return false;

  CComPtr<IDWriteFontResource> Resource;
  if (FAILED(DWriteFactory->CreateFontResource(File,0,&Resource)))
    return false;
  UINT32 iAxes = Resource->GetFontAxisCount();
  if (iAxes > 0)
  {
    Res.m_Axes.resize(iAxes);
    if (FAILED(Resource->GetFontAxisRanges(&Res.m_Axes[0],iAxes)))
      Res.m_Axes.clear();
  }
  Res.m_pResource = Resource.Detach();
  return true;
}

float Clamp(float Value, const DWRITE_FONT_AXIS_RANGE& Range)
{
  if (Value < Range.minValue)
    return Range.minValue;
  if (Value > Range.maxValue)
    return Range.maxValue;
  return Value;
}

// Create a font collection containing just the instance of the font for the face. For a
// variable font, the axes are set from the FDes entry, clamped to the axis ranges.
bool CreateFaceCollection(const BlorbResource& Res, CWinGlkBlorbFace& Face)
{
  std::vector<DWRITE_FONT_AXIS_VALUE> Values;
  for (size_t i = 0; i < Res.m_Axes.size(); i++)
  {
    const DWRITE_FONT_AXIS_RANGE& Range = Res.m_Axes[i];
    if (Range.minValue >= Range.maxValue)
      continue;
    DWRITE_FONT_AXIS_VALUE Value = { Range.axisTag, 0.0f };
    switch (Range.axisTag)
    {
    case DWRITE_FONT_AXIS_TAG_WEIGHT:
      Value.value = Clamp((float)Face.m_Weight,Range);
      break;
    case DWRITE_FONT_AXIS_TAG_ITALIC:
      Value.value = Clamp((Face.m_Style == 1) ? 1.0f : 0.0f,Range);
      break;
    case DWRITE_FONT_AXIS_TAG_SLANT:
      Value.value = Clamp((Face.m_Style == 2) ? -14.0f : 0.0f,Range);
      break;
    default:
      continue;
    }
    Values.push_back(Value);
  }

  CComPtr<IDWriteFontFaceReference1> Reference;
  if (FAILED(Res.m_pResource->CreateFontFaceReference(DWRITE_FONT_SIMULATIONS_NONE,
    Values.empty() ? NULL : &Values[0],(UINT32)Values.size(),&Reference)))
  {
    return false;
  }

  CComPtr<IDWriteFontSetBuilder2> Builder;
  if (FAILED(DWriteFactory->CreateFontSetBuilder(&Builder)))
    return false;
  if (FAILED(((IDWriteFontSetBuilder*)Builder)->AddFontFaceReference(Reference)))
    return false;
  CComPtr<IDWriteFontSet> Set;
  if (FAILED(((IDWriteFontSetBuilder*)Builder)->CreateFontSet(&Set)))
    return false;
  CComPtr<IDWriteFontCollection2> Collection;
  if (FAILED(DWriteFactory->CreateFontCollectionFromFontSet(Set,
    DWRITE_FONT_FAMILY_MODEL_WEIGHT_STRETCH_STYLE,&Collection)))
  {
    return false;
  }
  if (((IDWriteFontCollection*)Collection)->GetFontFamilyCount() < 1)
    return false;

  CComPtr<IDWriteFontFamily> Family;
  if (FAILED(((IDWriteFontCollection*)Collection)->GetFontFamily(0,&Family)))
    return false;
  CComPtr<IDWriteLocalizedStrings> Names;
  if (FAILED(Family->GetFamilyNames(&Names)))
    return false;
  UINT32 iIndex = 0;
  BOOL bExists = FALSE;
  if (FAILED(Names->FindLocaleName(Locale,&iIndex,&bExists)) || !bExists)
    iIndex = 0;
  UINT32 iNameLen = 0;
  if (FAILED(Names->GetStringLength(iIndex,&iNameLen)))
    return false;
  std::vector<WCHAR> Name(iNameLen+1);
  if (FAILED(Names->GetString(iIndex,&Name[0],iNameLen+1)))
    return false;

  Face.m_DWriteFamily = &Name[0];
  Face.m_pCollection = Collection.Detach();
  return true;
}

// Rank how well a face weight matches the desired weight, following the CSS font matching
// rules: lower ranks are better
int WeightRank(int iFace, int iWant)
{
  int iGroup = 0;
  if ((iWant >= 400) && (iWant <= 500))
  {
    if ((iFace >= iWant) && (iFace <= 500))
      iGroup = 0;
    else if (iFace < iWant)
      iGroup = 1;
    else
      iGroup = 2;
  }
  else if (iWant < 400)
    iGroup = (iFace <= iWant) ? 0 : 1;
  else
    iGroup = (iFace >= iWant) ? 0 : 1;
  return (iGroup * 10000) + abs(iFace - iWant);
}

} // namespace

void WinGlkBlorbFonts::Load(void)
{
  static bool bLoaded = false;
  static giblorb_map_t* pLoadedMap = NULL;

  giblorb_map_t* pMap = giblorb_get_resource_map();
  if (bLoaded && (pMap == pLoadedMap))
    return;
  bLoaded = true;
  pLoadedMap = pMap;

  // Fonts from any previous map are not freed, as device contexts may refer to them
  Faces.clear();
  Resources.clear();
  Fonts.clear();
  if (pMap == NULL)
    return;
  bool bDirectWrite = InitDirectWrite();

  giblorb_result_t Result;
  if (giblorb_load_chunk_by_type(pMap,giblorb_method_Memory,&Result,
    giblorb_make_id('F','D','e','s'),0) != giblorb_err_None)
  {
    return;
  }
  std::vector<BYTE> Desc((BYTE*)Result.data.ptr,(BYTE*)Result.data.ptr + Result.length);
  giblorb_unload_chunk(pMap,Result.chunknum);

  // Map of resource numbers to indexes in Resources, or -1 if the resource failed to load
  std::map<glui32,int> Loaded;

  size_t iPos = 4;
  DWORD iEntries = (Desc.size() >= 4) ? ReadBE32(&Desc[0]) : 0;
  for (DWORD i = 0; i < iEntries; i++)
  {
    if (iPos + 12 > Desc.size())
      break;
    glui32 number = ReadBE32(&Desc[iPos]);
    int weight = ReadBE16(&Desc[iPos+4]);
    int style = ReadBE16(&Desc[iPos+6]);
    DWORD length = ReadBE32(&Desc[iPos+8]);
    iPos += 12;
    if (length > Desc.size() - iPos)
      break;
    std::string Family = Utf8ToLowerAnsi((const char*)&Desc[iPos],length);
    iPos += length;

    if ((weight < 1) || (weight > 1000))
      weight = FW_NORMAL;
    if (style > 2)
      style = 0;
    if (Family.empty())
      continue;

    std::map<glui32,int>::const_iterator it = Loaded.find(number);
    if (it == Loaded.end())
    {
      bool bOk = false;
      BlorbResource Res;
      if (giblorb_load_resource(pMap,giblorb_method_Memory,&Result,
        giblorb_make_id('F','o','n','t'),number) == giblorb_err_None)
      {
        if ((Result.chunktype == giblorb_make_id('T','T','F',' ')) &&
          ReadTrueType((const BYTE*)Result.data.ptr,Result.length,Res))
        {
          if (bDirectWrite)
            bOk = LoadDirectWrite(Result.data.ptr,Result.length,Res);
          else
          {
            // GDI fonts added from memory are private to this process
            DWORD iInstalled = 0;
            bOk = (::AddFontMemResourceEx(Result.data.ptr,Result.length,NULL,&iInstalled) != 0);
          }
        }
        giblorb_unload_chunk(pMap,Result.chunknum);
      }
      if (bOk)
      {
        Resources.push_back(Res);
        Loaded[number] = (int)Resources.size()-1;
      }
      else
        Loaded[number] = -1;
      it = Loaded.find(number);
    }
    if (it->second < 0)
      continue;

    CWinGlkBlorbFace Face;
    Face.m_Family = Family;
    Face.m_Weight = weight;
    Face.m_Style = style;
    Face.m_Resource = it->second;
    Face.m_pCollection = NULL;
    if (bDirectWrite && !CreateFaceCollection(Resources[Face.m_Resource],Face))
      continue;
    Faces.push_back(Face);
  }
}

bool WinGlkBlorbFonts::UseDirectWrite(void)
{
  return InitDirectWrite();
}

const CWinGlkBlorbFace* WinGlkBlorbFonts::Match(const char* Family, int Weight, bool Italic)
{
  static const int ItalicOrder[] = { 1, 2, 0 };
  static const int NormalOrder[] = { 0, 2, 1 };
  const int* pOrder = Italic ? ItalicOrder : NormalOrder;

  for (int i = 0; i < 3; i++)
  {
    const CWinGlkBlorbFace* pBest = NULL;
    int iBestRank = 0;
    for (size_t j = 0; j < Faces.size(); j++)
    {
      const CWinGlkBlorbFace& Face = Faces[j];
      if ((Face.m_Style != pOrder[i]) || (Face.m_Family != Family))
        continue;
      int iRank = WeightRank(Face.m_Weight,Weight);
      if ((pBest == NULL) || (iRank < iBestRank))
      {
        pBest = &Face;
        iBestRank = iRank;
      }
    }
    if (pBest != NULL)
      return pBest;
  }
  return NULL;
}

const char* WinGlkBlorbFonts::GetGdiFont(const CWinGlkBlorbFace* pFace, int& Weight, bool& Italic)
{
  const BlorbResource& Res = Resources[pFace->m_Resource];

  // Synthesize bold or italic if the chosen face lacks it
  bool bBold = (Weight >= 600) && (pFace->m_Weight < 600);
  bool bItalic = Italic && (pFace->m_Style == 0);

  // GDI cannot set variation axes, so for a variable font ask for the declared weight
  // and style, and let GDI choose the nearest named instance
  if (Res.m_bVariable)
  {
    Weight = pFace->m_Weight;
    Italic = (pFace->m_Style != 0);
  }
  else
  {
    Weight = Res.m_GdiWeight;
    Italic = Res.m_bGdiItalic;
  }
  if (bBold && (Weight < FW_BOLD))
    Weight = FW_BOLD;
  if (bItalic)
    Italic = true;
  return Res.m_GdiFamily.c_str();
}

CWinGlkBlorbFont* WinGlkBlorbFonts::GetFont(const CWinGlkBlorbFace* pFace, int Weight, bool Italic,
  int Pixels, bool Underline)
{
  if (!InitDirectWrite() || (Pixels < 1))
    return NULL;

  char key[128];
  sprintf(key,"%p|%d|%d|%d|%d",(const void*)pFace,Weight,(int)Italic,Pixels,(int)Underline);
  std::map<std::string,CWinGlkBlorbFont*>::const_iterator it = Fonts.find(key);
  if (it != Fonts.end())
    return it->second;

  DWRITE_FONT_WEIGHT FontWeight = (DWRITE_FONT_WEIGHT)Weight;
  DWRITE_FONT_STYLE FontStyle = Italic ? DWRITE_FONT_STYLE_ITALIC : DWRITE_FONT_STYLE_NORMAL;

  // The face's collection holds just one font, and DirectWrite synthesizes bold or
  // oblique if the requested weight or style is beyond it
  CComPtr<IDWriteTextFormat> Format;
  if (FAILED(((IDWriteFactory*)DWriteFactory)->CreateTextFormat(pFace->m_DWriteFamily.c_str(),
    pFace->m_pCollection,FontWeight,FontStyle,DWRITE_FONT_STRETCH_NORMAL,(FLOAT)Pixels,Locale,&Format)))
  {
    Fonts[key] = NULL;
    return NULL;
  }
  Format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);

  CWinGlkBlorbFont* pFont = new CWinGlkBlorbFont;
  pFont->m_pFormat = Format.Detach();
  pFont->m_bUnderline = Underline;
  pFont->m_iPixels = Pixels;

  // Get the vertical metrics from the font
  int iAscent = Pixels, iDescent = Pixels/4, iLineGap = 0;
  CComPtr<IDWriteFontFamily> Family;
  if (SUCCEEDED(pFace->m_pCollection->GetFontFamily(0,&Family)))
  {
    CComPtr<IDWriteFont> Font;
    if (SUCCEEDED(Family->GetFirstMatchingFont(FontWeight,DWRITE_FONT_STRETCH_NORMAL,FontStyle,&Font)))
    {
      DWRITE_FONT_METRICS Metrics;
      Font->GetMetrics(&Metrics);
      if (Metrics.designUnitsPerEm > 0)
      {
        double scale = (double)Pixels / Metrics.designUnitsPerEm;
        iAscent = (int)ceil(Metrics.ascent * scale);
        iDescent = (int)ceil(Metrics.descent * scale);
        iLineGap = (int)floor((Metrics.lineGap * scale) + 0.5);
      }
    }
  }
  pFont->m_iAscent = iAscent;
  pFont->m_iDescent = iDescent;
  pFont->m_iLineGap = iLineGap;

  Fonts[key] = pFont;
  return pFont;
}

void WinGlkBlorbFonts::GetMetrics(CWinGlkBlorbFont* pFont, TEXTMETRIC& Metrics)
{
  Metrics.tmAscent = pFont->m_iAscent;
  Metrics.tmDescent = pFont->m_iDescent;
  Metrics.tmHeight = pFont->m_iAscent + pFont->m_iDescent;
  Metrics.tmInternalLeading = max(Metrics.tmHeight - pFont->m_iPixels,0);
  Metrics.tmExternalLeading = pFont->m_iLineGap;
}

bool CWinGlkBlorbFont::CreateLayout(LPCWSTR pText, int iCount, IDWriteTextLayout** ppLayout)
{
  if (FAILED(((IDWriteFactory*)DWriteFactory)->CreateTextLayout(pText,iCount,m_pFormat,
    100000.0f,100000.0f,ppLayout)))
  {
    return false;
  }
  if (m_bUnderline)
  {
    DWRITE_TEXT_RANGE Range = { 0, (UINT32)iCount };
    (*ppLayout)->SetUnderline(TRUE,Range);
  }
  return true;
}

float CWinGlkBlorbFont::GetWidth(LPCWSTR pText, int iCount)
{
  std::wstring Text(pText,iCount);
  std::map<std::wstring,float>::const_iterator it = m_Widths.find(Text);
  if (it != m_Widths.end())
    return it->second;

  float width = 0.0f;
  CComPtr<IDWriteTextLayout> Layout;
  if (CreateLayout(pText,iCount,&Layout))
  {
    DWRITE_TEXT_METRICS Metrics;
    if (SUCCEEDED(Layout->GetMetrics(&Metrics)))
      width = Metrics.widthIncludingTrailingWhitespace;
  }

  if (m_Widths.size() > 4096)
    m_Widths.clear();
  m_Widths[Text] = width;
  return width;
}

SIZE WinGlkBlorbFonts::GetTextExtent(CWinGlkBlorbFont* pFont, LPCWSTR pText, int iCount)
{
  SIZE Size = { 0, pFont->m_iAscent + pFont->m_iDescent };
  if (iCount > 0)
    Size.cx = (LONG)floor(pFont->GetWidth(pText,iCount) + 0.5f);
  return Size;
}

void WinGlkBlorbFonts::TextOut(CWinGlkBlorbFont* pFont, HDC hdc, int x, int y,
  LPCWSTR pText, int iCount)
{
  if (iCount <= 0)
    return;
  CComPtr<IDWriteTextLayout> Layout;
  if (!pFont->CreateLayout(pText,iCount,&Layout))
    return;

  int iWidth = GetTextExtent(pFont,pText,iCount).cx;
  int iHeight = pFont->m_iAscent + pFont->m_iDescent;
  if (::GetBkMode(hdc) == OPAQUE)
  {
    RECT Back = { x, y, x + iWidth, y + iHeight };
    ::ExtTextOutW(hdc,0,0,ETO_OPAQUE,&Back,NULL,0,NULL);
  }

  // Leave room for glyphs that overhang the text box, as in script fonts
  int iPad = iHeight;
  RECT Bind = { x - iPad, y - iPad, x + iWidth + iPad, y + iHeight + iPad };
  RECT Clip;
  if (::GetClipBox(hdc,&Clip) != ERROR)
  {
    if (!::IntersectRect(&Bind,&Bind,&Clip))
      return;
  }

  // Align the baseline of the first line with the baseline from the font metrics
  float baseline = (float)pFont->m_iAscent;
  DWRITE_LINE_METRICS Line;
  UINT32 iLines = 0;
  if (SUCCEEDED(Layout->GetLineMetrics(&Line,1,&iLines)) && (iLines > 0))
    baseline = Line.baseline;

  if (FAILED(RenderTarget->BindDC(hdc,&Bind)))
    return;
  RenderTarget->BeginDraw();
  COLORREF Colour = ::GetTextColor(hdc);
  CComPtr<ID2D1SolidColorBrush> Brush;
  if (SUCCEEDED(RenderTarget->CreateSolidColorBrush(D2D1::ColorF(GetRValue(Colour)/255.0f,
    GetGValue(Colour)/255.0f,GetBValue(Colour)/255.0f),&Brush)))
  {
    D2D1_POINT_2F Origin = D2D1::Point2F((FLOAT)(x - Bind.left),
      (FLOAT)(y + pFont->m_iAscent - Bind.top) - baseline);
    RenderTarget->DrawTextLayout(Origin,Layout,Brush);
  }
  RenderTarget->EndDraw();
}
