#pragma once

struct SpeexPreprocessState_; // Forward declaration
typedef struct SpeexPreprocessState_ SpeexPreprocessState;

class CSpeexPreprocess
{
public:
	CSpeexPreprocess(int sampleRate, int samplesPerBuffer);
	~CSpeexPreprocess();

	bool EnableAGC(bool enable);
	bool IsAGCEnabled();

	bool IsDenoiseEnabled();
	bool EnableDenoise(bool enable);

	int GetDenoiseLevel();
	bool SetDenoiseLevel(int level);

	void RunPreprocess(short* buffer);

private:
	SpeexPreprocessState* m_pState = nullptr;
};
