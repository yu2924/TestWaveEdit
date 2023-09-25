//
//  WaveCutListPlayer.cpp
//  TestWaveEdit_App
//
//  created by yu2924 on 2023-03-25
//

#include "WaveCutListPlayer.h"

class WaveCutListAudioSource : public juce::AudioSource
{
public:
	WaveCutListReader::Ptr cutListReader;
	juce::AudioSampleBuffer readBuffer;
	int maxBufferSize = 0;
	bool looping = true;
	WaveCutListAudioSource()
	{
		cutListReader = WaveCutListReader::createInstance();
	}
	void prepareReadBuffer()
	{
		const WaveCutList& cl = cutListReader->getWaveCutList();
		int cch = !cl.empty() ? cl.front().sourceFile->format.numChannels : 0;
		readBuffer.setSize(cch, maxBufferSize);
	}
	// juce::AudioSource
	virtual void prepareToPlay(int lbuf, double) override
	{
		maxBufferSize = lbuf;
		prepareReadBuffer();
	}
	virtual void releaseResources() override
	{
		readBuffer.setSize(0, 0);
		maxBufferSize = 0;
	}
	virtual void getNextAudioBlock(const juce::AudioSourceChannelInfo& asci) override
	{
		int cchb = readBuffer.getNumChannels();
		int ccho = asci.buffer->getNumChannels();
		int pos = 0; while(pos < asci.numSamples)
		{
			int64_t slen = cutListReader->getTotalLength(), spos = cutListReader->getPosition();
			if(slen <= 0) break;
			int lseg = (int)std::min(slen - spos, (int64_t)std::min(asci.numSamples - pos, readBuffer.getNumSamples()));
			cutListReader->read(readBuffer.getArrayOfWritePointers(), cchb, lseg);
			for(int cch = std::min(cchb, ccho), ich = 0; ich < cch; ++ich)
			{
				asci.buffer->copyFrom(ich, asci.startSample + pos, readBuffer, ich, 0, lseg);
			}
			if(slen <= (spos + lseg))
			{
				if(looping) cutListReader->setPosition(0);
				else break;
			}
			pos += lseg;
		}
	}
	// APIs
	void setWaveCutList(const WaveCutList& v)
	{
		cutListReader->setWaveCutList(v);
		prepareReadBuffer();
	}
	int64_t getLength() const
	{
		return cutListReader->getTotalLength();
	}
	int64_t getPosition() const
	{
		return cutListReader->getPosition();
	}
	void setPosition(int64_t v)
	{
		cutListReader->setPosition(v);
	}
	bool isLooping() const
	{
		return looping;
	}
	void setLooping(bool v)
	{
		looping = v;
	}
};

class WaveCutListPlayerImpl : public WaveCutListPlayer, public juce::AsyncUpdater, public WaveCutListDocument::Listener, public juce::AudioIODeviceCallback
{
public:
	juce::AudioDeviceManager& audioDeviceManager;
	WaveCutListDocument& document;
	std::unique_ptr<WaveCutListAudioSource> cutListAudioSource;
	std::unique_ptr<juce::ResamplingAudioSource> resamplingAudioSource;
	WaveFormat waveFormat = {};
	bool running = false;
	WaveCutListPlayerImpl(juce::AudioDeviceManager& adm, WaveCutListDocument& doc) : audioDeviceManager(adm), document(doc)
	{
		cutListAudioSource = std::make_unique<WaveCutListAudioSource>();
		updateContent();
		document.addListener(this);
		audioDeviceManager.addAudioCallback(this);
	}
	virtual ~WaveCutListPlayerImpl()
	{
		audioDeviceManager.removeAudioCallback(this);
		document.removeListener(this);
	}
	void updateContent()
	{
		juce::ScopedLock sl(audioDeviceManager.getAudioCallbackLock());
		waveFormat = document.getWaveFormat();
		cutListAudioSource->setWaveCutList(document.getWaveCutlist());
		if(resamplingAudioSource)
		{
			juce::AudioIODevice* dev = audioDeviceManager.getCurrentAudioDevice();
			double fsdev = dev ? dev->getCurrentSampleRate() : 0;
			double fssrc = waveFormat.sampleRate;
			double fsratio = ((0 < fsdev) && (0 < fssrc)) ? (fssrc / fsdev) : 1;
			resamplingAudioSource->setResamplingRatio(fsratio);
			resamplingAudioSource->flushBuffers();
		}
		if(running && (cutListAudioSource->getLength() <= 0)) setRunning(false);
	}
	// --------------------------------------------------------------------------------
	// WavePlayback
	virtual double getDuration() const override
	{
		return (0 < waveFormat.sampleRate) ? ((double)cutListAudioSource->getLength() / waveFormat.sampleRate) : 0;
	}
	virtual bool isLooping() const override
	{
		return cutListAudioSource->isLooping();
	}
	virtual void setLooping(bool v) override
	{
		cutListAudioSource->setLooping(v);
		sendChangeMessage();
	}
	virtual double getPosition() const override
	{
		return (0 < waveFormat.sampleRate) ? ((double)cutListAudioSource->getPosition() / waveFormat.sampleRate) : 0;
	}
	virtual void setPosition(double v) override
	{
		juce::ScopedLock sl(audioDeviceManager.getAudioCallbackLock());
		int64_t spos = (int64_t)(v * waveFormat.sampleRate);
		cutListAudioSource->setPosition(spos);
		sendChangeMessage();
	}
	virtual bool isRunning() const override
	{
		return running;
	}
	virtual void setRunning(bool v) override
	{
		juce::ScopedLock sl(audioDeviceManager.getAudioCallbackLock());
		bool run = document.hasValidContent() ? v : false;
		if(run && (cutListAudioSource->getLength() <= cutListAudioSource->getPosition())) setPosition(0);
		running = run;
		if(!running && resamplingAudioSource) resamplingAudioSource->flushBuffers();
		sendChangeMessage();
	}
	// --------------------------------------------------------------------------------
	// juce::AsyncUpdater
	virtual void handleAsyncUpdate() override
	{
		setRunning(false);
	}
	// --------------------------------------------------------------------------------
	// WaveCutListDocument::Listener
	virtual void waveCutListDocumentDidInit(WaveCutListDocument*) override
	{
		updateContent();
		setPosition(0);
	}
	virtual void waveCutListDocumentDidReplaceSourceFile(WaveCutListDocument*, WaveSourceFile::Ptr, WaveSourceFile::Ptr) override
	{
	}
	virtual void waveCutListDocumentDidEdit(WaveCutListDocument*, int, const Range64&) override
	{
		updateContent();
	}
	// --------------------------------------------------------------------------------
	// juce::AudioIODeviceCallback
	virtual void audioDeviceIOCallbackWithContext(const float* const*, int, float* const* ppo, int ccho, int len, const juce::AudioIODeviceCallbackContext&) override
	{
		juce::AudioSampleBuffer outbuffer(ppo, ccho, len);
		outbuffer.clear();
		if(running)
		{
			resamplingAudioSource->getNextAudioBlock(juce::AudioSourceChannelInfo(outbuffer));
			if(!cutListAudioSource->isLooping() && (cutListAudioSource->getLength() <= cutListAudioSource->getPosition())) triggerAsyncUpdate();
		}
	}
	virtual void audioDeviceAboutToStart(juce::AudioIODevice* dev) override
	{
		int cchdev = dev->getActiveOutputChannels().countNumberOfSetBits();
		resamplingAudioSource = std::make_unique<juce::ResamplingAudioSource>(cutListAudioSource.get(), false, cchdev);
		resamplingAudioSource->prepareToPlay(dev->getCurrentBufferSizeSamples(), dev->getCurrentSampleRate());
		double fsdev = dev->getCurrentSampleRate();
		double fssrc = waveFormat.sampleRate;
		double fsratio = ((0 < fsdev) && (0 < fssrc)) ? (fssrc / fsdev) : 1;
		resamplingAudioSource->setResamplingRatio(fsratio);
	}
	virtual void audioDeviceStopped() override
	{
		if(resamplingAudioSource) resamplingAudioSource->releaseResources();
		resamplingAudioSource = nullptr;
	}
};

std::unique_ptr<WaveCutListPlayer> WaveCutListPlayer::createInstance(juce::AudioDeviceManager& adm, WaveCutListDocument& doc)
{
	return std::make_unique<WaveCutListPlayerImpl>(adm, doc);
}
