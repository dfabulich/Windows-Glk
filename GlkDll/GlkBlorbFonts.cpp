/////////////////////////////////////////////////////////////////////////////
//
// Windows MFC Glk Libraries
//
// GlkBlorbFonts
// Fonts from the Blorb resource map
//
/////////////////////////////////////////////////////////////////////////////

// This file does not use the precompiled header, as it does not need MFC.
#include <windows.h>

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
};

namespace {

// A font resource loaded from the Blorb file
struct BlorbResource
{
  BlorbResource() : m_GdiWeight(FW_NORMAL), m_bGdiItalic(false), m_bVariable(false) {}

  // Details for drawing with GDI
  std::string m_GdiFamily;
  int m_GdiWeight;
  bool m_bGdiItalic;
  bool m_bVariable;
};

std::vector<CWinGlkBlorbFace> Faces;
std::vector<BlorbResource> Resources;

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
  if (pMap == NULL)
    return;

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
          // GDI fonts added from memory are private to this process
          DWORD iInstalled = 0;
          bOk = (::AddFontMemResourceEx(Result.data.ptr,Result.length,NULL,&iInstalled) != 0);
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
    Faces.push_back(Face);
  }
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
