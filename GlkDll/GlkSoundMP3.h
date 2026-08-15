/////////////////////////////////////////////////////////////////////////////
//
// Windows MFC GLK Libraries
//
// GlkSoundMP3
// GLK MP3 sounds
//
/////////////////////////////////////////////////////////////////////////////

#ifndef WINGLK_SOUND_MP3_H_
#define WINGLK_SOUND_MP3_H_

#include "GlkSound.h"
#include "DSoundEngine.h"

/////////////////////////////////////////////////////////////////////////////
// Class for MP3 sound loader
/////////////////////////////////////////////////////////////////////////////

class CWinGlkMP3SoundLoader : public CWinGlkSoundLoader
{
public:
  CWinGlkMP3SoundLoader() {}
  virtual ~CWinGlkMP3SoundLoader() {}

public:
  // Get file prefix for sounds supported by this loader
  virtual LPCTSTR GetFilePrefix(void);

  // Get the file extensions for sounds supported for this loader
  virtual int GetNumberFileExtensions(void);
  virtual LPCTSTR GetFileExtension(int iExtIndex);

  // Get the identifier for sounds supported for this loader
  virtual glui32 GetIdentifier(void);

  // Get a sound object
  virtual CWinGlkSound* GetSound(LPCTSTR pszFileName);
  virtual CWinGlkSound* GetSound(BYTE* pData, int iLength);
};

/////////////////////////////////////////////////////////////////////////////
// Class for MP3 sounds
/////////////////////////////////////////////////////////////////////////////

class CWinGlkMP3Sound : public CWinGlkSound, public CDSound
{
  DECLARE_DYNAMIC(CWinGlkMP3Sound)

public:
  CWinGlkMP3Sound(BYTE* pData, int iLength);
  CWinGlkMP3Sound(LPCTSTR pszFileName);
  virtual ~CWinGlkMP3Sound();

  virtual bool Play(int iRepeat, int iVolume, bool PauseState);
  virtual bool IsPlaying(void);
  virtual void Pause(bool PauseState);
  virtual void SetVolume(int iVolume);

  virtual void WriteSampleData(unsigned char* pSample, int iSampleLen);
  virtual bool IsSoundOver(DWORD Tick);
  virtual int GetType(void);

protected:
  // Private implementation data
  struct Impl;
  Impl* m_Impl;

  // Whether the decoder has been opened
  bool m_DecoderOpen;
  // The duration of the sample
  int m_Duration;
};

#endif // WINGLK_SOUND_MP3_H_
