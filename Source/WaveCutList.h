//
//  WaveCutList.h
//  TestWaveEdit_App
//
//  created by yu2924 on 2023-03-30
//

#pragma once

#include <JuceHeader.h>

struct WaveFormat
{
	double sampleRate;
	int numChannels;
	bool operator==(const WaveFormat& r) const { return (sampleRate == r.sampleRate) && (numChannels == r.numChannels); }
	bool operator!=(const WaveFormat& r) const { return (sampleRate != r.sampleRate) || (numChannels != r.numChannels); }
};

struct Range64
{
	int64_t begin = 0;
	int64_t end = 0;
	int64_t size() const { return end - begin; }
	bool isEmpty() const { return end <= begin; }
	bool intersects(int64_t t) const { return (begin <= t) && (t < end); }
	bool intersects(const Range64& r) const { return intersects(r.begin, r.end); }
	bool intersects(int64_t rbegin, int64_t rend) const { return std::max(begin, rbegin) < std::min(end, rend); }
	Range64 intersection(const Range64& r) const { return intersection(r.begin, r.end); }
	Range64 intersection(int64_t rbegin, int64_t rend) const
	{
		int64_t b = std::max(begin, rbegin);
		int64_t e = std::min(end, rend);
		return (b < e) ? Range64{ b, e } : Range64{};
	}
};

class WaveSourceFile : public juce::ReferenceCountedObject
{
public:
	using Ptr = juce::ReferenceCountedObjectPtr<WaveSourceFile>;
	juce::File backingFile;
	int64_t length = 0;
	WaveFormat format = {};
	virtual bool read(float* const* pp, int cch, int64_t samplepos, int len) = 0;
};

class ArchivedWaveSourceFile : public WaveSourceFile
{
protected:
	ArchivedWaveSourceFile() {}
public:
	static Ptr createInstance(juce::AudioFormatManager& afm, const juce::File& path);
	static Ptr createInstance(std::unique_ptr<juce::AudioFormatReader> reader, const juce::File& path);
};

class TemporaryWaveSourceFile : public WaveSourceFile
{
protected:
	TemporaryWaveSourceFile() {}
public:
	static juce::File getTempDirectory();
	static juce::File getNextUniquePath();
	static std::unique_ptr<juce::AudioFormatWriter> createCompatibleAudioFromatWriter(const juce::File& path, const WaveFormat& fmt);
	static Ptr createInstanceFromSourceFile(WaveSourceFile::Ptr src);
	static Ptr createInstanceFromCompatiblePath(const juce::File& wavpath);
};

struct WaveCut
{
	WaveSourceFile::Ptr sourceFile;
	Range64 range;
};

class WaveCutList : public std::list<WaveCut>
{
public:
	int64_t calcTotalSize() const;
	WaveCutList intersectRange(Range64 oprange) const;
	bool insertList(const WaveCutList& clinsert, int64_t inspoint);
	void eraseRange(Range64 oprange);
	void mergeAdjucentContinuousCuts();
};

class WaveCutListReader : public juce::ReferenceCountedObject
{
protected:
	WaveCutListReader() {}
public:
	using Ptr = juce::ReferenceCountedObjectPtr<WaveCutListReader>;
	virtual ~WaveCutListReader() {}
	virtual const WaveCutList& getWaveCutList() const = 0;
	virtual void setWaveCutList(const WaveCutList& v) = 0;
	virtual int64_t getTotalLength() const = 0;
	virtual int64_t getPosition() const = 0;
	virtual void setPosition(int64_t v) = 0;
	virtual bool read(float* const* pp, int cch, int len) = 0;
	static Ptr createInstance();
};

// TODO: asynchronous processing, applying arbitrary gain envelope, etc.
class WaveCutListModifier
{
protected:
	WaveCutListModifier() {}
public:
	static WaveCutList processSyncWithRamp(const WaveCutList& srccl, const Range64& r, float startgain, float stopgain);
};
