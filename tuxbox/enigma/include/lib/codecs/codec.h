#ifndef DISABLE_FILE

#ifndef __codec_h
#define __codec_h

#include <libsig_comp.h>

class eIOBuffer;

class eAudioDecoder
{
protected:
	int speed;
public:
	eAudioDecoder();
	virtual ~eAudioDecoder();

	virtual int decodeMore(int last, int maxsamples, Signal1<void,unsigned int>*cb=0)=0; // returns number of samples(!) written to IOBuffer (out)
	virtual void resync()=0; // clear (secondary) decoder buffers

	struct pcmSettings
	{
		unsigned int samplerate;
		unsigned int channels;
		unsigned int format;
		int reconfigure;
	} pcmsettings;
	virtual int getMinimumFramelength()=0;
	void setSpeed(int _speed) { speed=_speed; }	
	virtual int getAverageBitrate()=0;
};

#endif

#endif //DISABLE_FILE
