/////////////////////////////////////////////////////////////////////////////
//
// Windows MFC Glk Libraries
//
// GlkSoundMP3
// Glk MP3 sounds
//
/////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "GlkSoundMP3.h"
#include "GlkTime.h"
#include <math.h>

extern "C"
{
#include "gi_blorb.h"
}

#define MINIMP3_ONLY_MP3
#define MINIMP3_NO_SIMD
#define MINIMP3_NO_STDIO
#define MINIMP3_IMPLEMENTATION
#pragma warning(push)
#pragma warning(disable : 4244 4456)
#include "minimp3_ex.h"
#pragma warning(pop)

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// Class for MP3 sound loader
/////////////////////////////////////////////////////////////////////////////

// Get file prefix for sounds supported by this loader
LPCTSTR CWinGlkMP3SoundLoader::GetFilePrefix(void)
{
  return "mus";
}

// Get the file extensions for sounds supported for this loader
int CWinGlkMP3SoundLoader::GetNumberFileExtensions(void)
{
  return 1;
}

LPCTSTR CWinGlkMP3SoundLoader::GetFileExtension(int iExtIndex)
{
  return "mp3";
}

// Get the identifier for sounds supported for this loader
glui32 CWinGlkMP3SoundLoader::GetIdentifier(void)
{
  return giblorb_make_id('M','P','3',' ');
}

// Get a sound object
CWinGlkSound* CWinGlkMP3SoundLoader::GetSound(LPCTSTR pszFileName)
{
  return new CWinGlkMP3Sound(pszFileName);
}

CWinGlkSound* CWinGlkMP3SoundLoader::GetSound(BYTE* pData, int iLength)
{
  return new CWinGlkMP3Sound(pData,iLength);
}

/////////////////////////////////////////////////////////////////////////////
// Class for MP3 sounds
/////////////////////////////////////////////////////////////////////////////

struct CWinGlkMP3Sound::Impl
{
  mp3dec_ex_t Decoder;
};

IMPLEMENT_DYNAMIC(CWinGlkMP3Sound,CWinGlkSound);

CWinGlkMP3Sound::CWinGlkMP3Sound(BYTE* pData, int iLength) : CWinGlkSound(pData,iLength)
{
  m_Impl = new Impl();
  m_DecoderOpen = false;
  m_Duration = 0;
}

CWinGlkMP3Sound::CWinGlkMP3Sound(LPCTSTR pszFileName) : CWinGlkSound(pszFileName)
{
  m_Impl = new Impl();
  m_DecoderOpen = false;
  m_Duration = 0;
}

CWinGlkMP3Sound::~CWinGlkMP3Sound()
{
  RemoveFromList();
  if (m_DecoderOpen)
    mp3dec_ex_close(&(m_Impl->Decoder));
  delete m_Impl;
}

bool CWinGlkMP3Sound::Play(int iRepeat, int iVolume, bool PauseState)
{
  // Open the MP3 decoder
  if (mp3dec_ex_open_buf(&(m_Impl->Decoder),m_pData,m_iLength,MP3D_SEEK_TO_SAMPLE))
    return false;
  m_DecoderOpen = true;

  // Create a buffer
  if (CreateBuffer(m_Impl->Decoder.info.channels,m_Impl->Decoder.info.hz,16) == false)
    return false;

  // Set the duration of the sample
  if (iRepeat > 0)
  {
    double dSamplesPerSec = m_Impl->Decoder.info.channels * m_Impl->Decoder.info.hz;
    m_Duration = (DWORD)ceil((1000.0 * iRepeat * m_Impl->Decoder.samples) / dSamplesPerSec);
  }
  else
    m_Duration = -1;

  // Fill the buffer with sample data
  m_iRepeat = (iRepeat < 0) ? -1 : iRepeat - 1;
  if (FillBuffer(GetBufferSize()) == false)
    return false;

  // Set the volume for the buffer
  SetVolume(iVolume);

  // Start the buffer playing
  return PlayBuffer(PauseState);
}

bool CWinGlkMP3Sound::IsPlaying(void)
{
  return m_Active;
}

void CWinGlkMP3Sound::Pause(bool PauseState)
{
  CDSound::Pause(PauseState);

  DWORD now = ::GetTickCount();
  CSingleLock Lock(CDSoundEngine::GetSoundLock(),TRUE);

  // If pausing, reduce the sound duration by the amount already played
  if (PauseState)
  {
    if (m_Duration > 0)
    {
      m_Duration -= TickCountDiff(now,m_StartTime);
      if (m_Duration < 0)
        m_Duration = 0;
    }
  }

  // Update the start time to now when pausing or unpausing
  m_StartTime = now;
}

void CWinGlkMP3Sound::SetVolume(int iVolume)
{
  // The SetVolume() call to DirectSound requires a volume
  // in 100ths of a decibel.
  SetBufferVolume((LONG)DecibelVolume(iVolume) * 100L);
}

// Write sample data into the supplied PCM sample buffers
void CWinGlkMP3Sound::WriteSampleData(unsigned char* pSample, int iSampleLen)
{
  int iCurrent = 0;
  while (iCurrent < iSampleLen)
  {
    size_t szRead = mp3dec_ex_read(&(m_Impl->Decoder),
      (mp3d_sample_t *)pSample,iSampleLen / sizeof(mp3d_sample_t));
    szRead *= sizeof(mp3d_sample_t);
    if (szRead > 0)
      iCurrent += szRead;
    else
    {
      if (m_iRepeat > 0)
      {
        mp3dec_ex_seek(&(m_Impl->Decoder),0);
        m_iRepeat--;
      }
      else if (m_iRepeat == -1)
        mp3dec_ex_seek(&(m_Impl->Decoder),0);
      else
      {
        while (iCurrent < iSampleLen)
          pSample[iCurrent++] = 0;
      }
    }
  }
}

// Check if the sound has finished playing
bool CWinGlkMP3Sound::IsSoundOver(DWORD Tick)
{
  if (m_Active == false)
    return true;

  // Check if sound is paused
  if ((GetStatus() & DSBSTATUS_PLAYING) == 0)
    return false;

  // Check if sound is playing forever
  if (m_Duration < 0)
    return false;

  return (Tick > m_StartTime + m_Duration);
}

// Get a type identifier for the sound
int CWinGlkMP3Sound::GetType(void)
{
  return (int)'3';
}
