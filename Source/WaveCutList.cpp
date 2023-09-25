//
//  WaveCutList.cpp
//  TestWaveEdit_App
//
//  created by yu2924 on 2023-03-30
//

#include "WaveCutList.h"

// ================================================================================
// WaveSourceFile

class ArchivedWaveSourceFileImpl : public ArchivedWaveSourceFile
{
public:
	std::unique_ptr<juce::AudioFormatReader> formatReader;
	ArchivedWaveSourceFileImpl(std::unique_ptr<juce::AudioFormatReader> reader, const juce::File& path)
	{
		formatReader.reset(reader.release());
		if(!formatReader) return;
		backingFile = path;
		length = formatReader->lengthInSamples;
		format = { formatReader->sampleRate, (int)formatReader->numChannels };
	}
	virtual bool read(float* const* pp, int cch, int64_t samplepos, int len) override
	{
		if(!formatReader) return false;
		if(cch != format.numChannels) return false;
		return formatReader->read(pp, cch, samplepos, len);
	}
};

WaveSourceFile::Ptr ArchivedWaveSourceFile::createInstance(juce::AudioFormatManager& afm, const juce::File& path)
{
	std::unique_ptr<juce::AudioFormatReader> reader(afm.createReaderFor(path));
	if(!reader) return nullptr;
	juce::ReferenceCountedObjectPtr<ArchivedWaveSourceFileImpl> ptr = new ArchivedWaveSourceFileImpl(std::move(reader), path);
	if(!ptr->formatReader) return nullptr;
	return ptr;
}

WaveSourceFile::Ptr ArchivedWaveSourceFile::createInstance(std::unique_ptr<juce::AudioFormatReader> reader, const juce::File& path)
{
	juce::ReferenceCountedObjectPtr<ArchivedWaveSourceFileImpl> ptr = new ArchivedWaveSourceFileImpl(std::move(reader), path);
	if(!ptr->formatReader) return nullptr;
	return ptr;
}

class TemporaryWaveSourceFileImpl : public TemporaryWaveSourceFile
{
public:
	static std::unique_ptr<juce::AudioFormatReader> createAudioFormatReader(const juce::File& path)
	{
		std::unique_ptr<juce::FileInputStream> str = std::make_unique<juce::FileInputStream>(path);
		if(str->failedToOpen()) return nullptr;
		juce::WavAudioFormat wavfmt;
		std::unique_ptr<juce::AudioFormatReader> reader(wavfmt.createReaderFor(str.get(), false));
		if(!reader) return nullptr;
		str.release();
		return reader;
	}
	std::unique_ptr<juce::AudioFormatReader> formatReader;
	TemporaryWaveSourceFileImpl(WaveSourceFile::Ptr src)
	{
		jassert(src != nullptr);
		if(!src) return;
		juce::File path = getNextUniquePath();
		WaveFormat fmt = src->format;
		{
			std::unique_ptr<juce::AudioFormatWriter> writer = createCompatibleAudioFromatWriter(path, src->format);
			if(!writer) return;
			juce::AudioBuffer<float> buf(src->format.numChannels, 16384);
			int64_t len = src->length, pos = 0; while(pos < len)
			{
				int lseg = (int)std::min(len - pos, (int64_t)buf.getNumSamples());
				if(!src->read(buf.getArrayOfWritePointers(), src->format.numChannels, pos, lseg)) return;
				if(!writer->writeFromAudioSampleBuffer(buf, 0, lseg)) return;
				pos += lseg;
			}
		}
		formatReader = createAudioFormatReader(path);
		if(!formatReader) return;
		backingFile = path;
		length = formatReader->lengthInSamples;
		format = { formatReader->sampleRate, (int)formatReader->numChannels };
	}
	TemporaryWaveSourceFileImpl(const juce::File& wavpath)
	{
		formatReader = createAudioFormatReader(wavpath);
		if(!formatReader) return;
		backingFile = wavpath;
		length = formatReader->lengthInSamples;
		format = { formatReader->sampleRate, (int)formatReader->numChannels };
	}
	virtual ~TemporaryWaveSourceFileImpl()
	{
		formatReader = nullptr;
		if(backingFile.exists()) backingFile.deleteFile();
	}
	virtual bool read(float* const* pp, int cch, int64_t samplepos, int len) override
	{
		if(!formatReader) return false;
		if(cch != format.numChannels) return false;
		return formatReader->read(pp, cch, samplepos, len);
	}
};

juce::File TemporaryWaveSourceFile::getTempDirectory()
{
	return juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("{FDD934D4-57DF-415D-83ED-EFC251C75C4D}");
}

juce::File TemporaryWaveSourceFile::getNextUniquePath()
{
	juce::File path = getTempDirectory().getNonexistentChildFile(juce::String::formatted("%08u", juce::Random::getSystemRandom().nextInt()), ".wav", true);
	path.create();
	return path;
}

std::unique_ptr<juce::AudioFormatWriter> TemporaryWaveSourceFile::createCompatibleAudioFromatWriter(const juce::File& path, const WaveFormat& fmt)
{
	std::unique_ptr<juce::FileOutputStream> str = std::make_unique<juce::FileOutputStream>(path);
	if(str->failedToOpen()) return nullptr;
	str->setPosition(0);
	str->truncate();
	juce::WavAudioFormat wavfmt;
	std::unique_ptr<juce::AudioFormatWriter> writer(wavfmt.createWriterFor(str.get(), fmt.sampleRate, fmt.numChannels, 32, {}, 0));
	if(!writer) return nullptr;
	str.release();
	return writer;
}

WaveSourceFile::Ptr TemporaryWaveSourceFile::createInstanceFromSourceFile(WaveSourceFile::Ptr src)
{
	juce::ReferenceCountedObjectPtr<TemporaryWaveSourceFileImpl> ptr = new TemporaryWaveSourceFileImpl(src);
	if(!ptr->formatReader) return nullptr;
	return ptr;
}

WaveSourceFile::Ptr TemporaryWaveSourceFile::createInstanceFromCompatiblePath(const juce::File& wavpath)
{
	juce::ReferenceCountedObjectPtr<TemporaryWaveSourceFileImpl> ptr = new TemporaryWaveSourceFileImpl(wavpath);
	if(!ptr->formatReader) return nullptr;
	return ptr;
}

// ================================================================================
// WaveCutList

int64_t WaveCutList::calcTotalSize() const
{
	int64_t l = 0; for(const auto& wc : *this) l += wc.range.size();
	return l;
}

WaveCutList WaveCutList::intersectRange(Range64 oprange) const
{
	WaveCutList clresult;
	int64_t offset = 0;
	for(const auto& wc : *this)
	{
		Range64 rtile{ offset, offset + wc.range.size() };
		Range64 rx = oprange.intersection(rtile);
		if(!rx.isEmpty())
		{
			int64_t begintrim = rx.begin - rtile.begin;
			int64_t endtrim = rtile.end - rx.end;
			clresult.push_back({ wc.sourceFile, { wc.range.begin + begintrim, wc.range.end - endtrim } });
		}
		offset += rtile.size();
	}
	return clresult;
}

bool WaveCutList::insertList(const WaveCutList& clinsert, int64_t inspoint)
{
	bool rins = false;
	int64_t offset = 0;
	for(iterator it = begin(); it != end(); ++it)
	{
		Range64 rtile{ offset, offset + it->range.size() };
		if(rtile.intersects(inspoint))
		{
			int64_t lbefore = inspoint - rtile.begin;
			if(0 < lbefore) // split into two
			{
				iterator itb = insert(it, *it);
				itb->range.end = itb->range.begin + lbefore;
				it->range.begin += lbefore;
			}
			insert(it, clinsert.begin(), clinsert.end());
			rins = true;
			break;
		}
		offset += rtile.size();
	}
	if(!rins && (offset == inspoint))
	{
		insert(end(), clinsert.begin(), clinsert.end());
		rins = true;
	}
	return rins;
}

void WaveCutList::eraseRange(Range64 oprange)
{
	int64_t offset = 0;
	iterator it = begin();
	while(it != end())
	{
		Range64 rtile{ offset, offset + it->range.size() };
		Range64 rx = oprange.intersection(rtile);
		if(!rx.isEmpty()) // at least partially intersect
		{
			if(rtile.size() == rx.size()) // entirely intersect
			{
				it = erase(it);
			}
			else // partially intersect
			{
				int64_t lbegin = rx.begin - rtile.begin;
				int64_t lend = rtile.end - rx.end;
				if((0 < lbegin) && (lend <= 0))  // rear intersect: trim end
				{
					it->range.end = it->range.begin + lbegin;
				}
				else if((0 < lend) && (lbegin <= 0)) // front intersect: trim begin
				{
					it->range.begin = it->range.end - lend;
				}
				else // center intersect: split into two
				{
					iterator itb = insert(it, *it);
					itb->range.end = itb->range.begin + lbegin;
					it->range.begin = it->range.end - lend;
				}
				++it;
			}
		}
		else ++it; // not intersect
		offset += rtile.size();
	}
}

void WaveCutList::mergeAdjucentContinuousCuts()
{
	WaveCutList::iterator it = begin();
	while(it != end())
	{
		iterator itnext = std::next(it); if(itnext == end()) break;
		if((it->sourceFile == itnext->sourceFile) && (it->range.end == itnext->range.begin))
		{
			it->range.end = itnext->range.end;
			erase(itnext);
		}
		else it = itnext;
	}
}

// ================================================================================
// WaveCutListReader

class WaveCutListReaderImpl : public WaveCutListReader
{
public:
	WaveCutList waveCutList;
	int numChannels = 0;
	struct
	{
		WaveCutList::iterator iterator;
		int64_t offset;
	} currentCut;
	int64_t totalLength = 0;
	int64_t position = 0;
	std::vector<float*> ptrArray;
	WaveCutListReaderImpl() : currentCut{ waveCutList.end(), 0 }
	{
	}
	virtual ~WaveCutListReaderImpl()
	{
	}
	virtual const WaveCutList& getWaveCutList() const override
	{
		return waveCutList;
	}
	virtual void setWaveCutList(const WaveCutList& v) override
	{
		waveCutList = v;
		numChannels = waveCutList.empty() ? 0 : waveCutList.front().sourceFile->format.numChannels;
		totalLength = waveCutList.calcTotalSize();
		ptrArray.resize(numChannels);
		setPosition(position);
	}
	virtual int64_t getTotalLength() const override
	{
		return totalLength;
	}
	virtual int64_t getPosition() const override
	{
		return position;
	}
	virtual void setPosition(int64_t v) override
	{
		position = std::max((int64_t)0, std::min(totalLength, v));
		currentCut = { waveCutList.end(), 0 };
		int64_t offset = 0;
		for(WaveCutList::iterator it = waveCutList.begin(); it != waveCutList.end(); ++it)
		{
			Range64 rng{ offset, offset + it->range.size() };
			if(rng.intersects(position))
			{
				currentCut = { it, offset };
				break;
			}
			offset += it->range.size();
		}
	}
	virtual bool read(float* const* pp, int cch, int len) override
	{
		if((currentCut.iterator == waveCutList.end()) || (numChannels <= 0)) return false;
		if(cch != numChannels) return false;
		for(int ich = 0; ich < cch; ++ich) ptrArray[ich] = pp[ich];
		int pos = 0;
		while(pos < len)
		{
			if(currentCut.iterator == waveCutList.end()) break;
			const WaveCut& wc = *currentCut.iterator;
			int lseg = (int)std::min(currentCut.offset + wc.range.size() - position, (int64_t)(len - pos));
			wc.sourceFile->read(ptrArray.data(), cch, wc.range.begin + position - currentCut.offset, lseg);
			for(int ich = 0; ich < cch; ++ich) ptrArray[ich] += lseg;
			pos += lseg;
			position += lseg;
			if(currentCut.offset + wc.range.size() <= position)
			{
				++currentCut.iterator;
				currentCut.offset += wc.range.size();
			}
		}
		return true;
	}
};

WaveCutListReader::Ptr WaveCutListReader::createInstance()
{
	return new WaveCutListReaderImpl;
}

// ================================================================================
// WaveCutListModifier

WaveCutList WaveCutListModifier::processSyncWithRamp(const WaveCutList& srccl, const Range64& r, float startgain, float stopgain)
{
	if(srccl.empty()) return {};
	WaveFormat fmt = srccl.front().sourceFile->format;
	WaveCutListReader::Ptr reader = WaveCutListReader::createInstance();
	if(!reader) return {};
	reader->setWaveCutList(srccl);
	reader->setPosition(r.begin);
	juce::File path = TemporaryWaveSourceFileImpl::getNextUniquePath();
	{
		std::unique_ptr<juce::AudioFormatWriter> writer = TemporaryWaveSourceFile::createCompatibleAudioFromatWriter(path, fmt);
		if(!writer) return {};
		juce::AudioBuffer<float> buf(fmt.numChannels, 16384);
		int64_t pos = r.begin; while(pos < r.end)
		{
			int lseg = (int)std::min(r.end - pos, (int64_t)buf.getNumSamples());
			reader->read(buf.getArrayOfWritePointers(), buf.getNumChannels(), lseg);
			float g0 = (float)(startgain + (stopgain - startgain) * (double)(pos - r.begin) / (double)r.size());
			float g1 = (float)(startgain + (stopgain - startgain) * (double)(pos + lseg - r.begin) / (double)r.size());
			buf.applyGainRamp(0, lseg, g0, g1);
			writer->writeFromAudioSampleBuffer(buf, 0, lseg);
			pos += lseg;
		}
	}
	WaveSourceFile::Ptr tmpfile = TemporaryWaveSourceFile::createInstanceFromCompatiblePath(path);
	if(!tmpfile) return {};
	return { { { tmpfile, { 0, tmpfile->length } } } };
}
