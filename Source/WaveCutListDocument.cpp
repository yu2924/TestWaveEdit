//
//  WaveCutListDocument.cpp
//  TestWaveEdit_App
//
//  created by yu2924 on 2023-03-25
//

#include "WaveCutListDocument.h"

class WaveCutListClipboard
{
public:
	WaveCutList cutList;
	bool isEmpty() const { return cutList.empty(); }
	void clear() { cutList.clear(); }
	WaveFormat getFormat() const { return !cutList.empty() ? cutList.front().sourceFile->format : WaveFormat(); }
	const WaveCutList& getCutList() const { return cutList; }
	void setCutList(const WaveCutList& cl)
	{
		cutList = cl;
		DBG("[WaveCutListClipboard] set: cutlistsize=" << (int)cutList.size() << " totallength=" << cutList.calcTotalSize());
	}
};

class WaveInsertUndoAction : public juce::UndoableAction
{
public:
	WaveCutList& targetCutList;
	WaveCutList insertCutList;
	Range64 insertionRange;
	WaveInsertUndoAction(WaveCutList& target, const WaveCutList& insert, int64_t t) : targetCutList(target), insertCutList(insert), insertionRange{ t, t + insert.calcTotalSize() }
	{
	}
	virtual bool perform() override
	{
		targetCutList.insertList(insertCutList, insertionRange.begin);
		return true;
	}
	virtual bool undo() override
	{
		targetCutList.eraseRange(insertionRange);
		return true;
	}
};

class WaveEraseUndoAction : public juce::UndoableAction
{
public:
	WaveCutList& taregtCutList;
	WaveCutList eraseCutList;
	Range64 eraseRange;
	WaveEraseUndoAction(WaveCutList& target, const Range64 cr) : taregtCutList(target), eraseRange(cr)
	{
	}
	virtual bool perform() override
	{
		eraseCutList = taregtCutList.intersectRange(eraseRange);
		taregtCutList.eraseRange(eraseRange);
		return true;
	}
	virtual bool undo() override
	{
		taregtCutList.insertList(eraseCutList, eraseRange.begin);
		return true;
	}
};

class WaveCutListDocumentImpl : public WaveCutListDocument
{
public:
	struct ScopedUndoTransaction
	{
		juce::UndoManager& undoManager;
		ScopedUndoTransaction(juce::UndoManager& um, const juce::String& n) : undoManager(um) { undoManager.beginNewTransaction(n); }
		~ScopedUndoTransaction() { undoManager.beginNewTransaction(); }
	};
	juce::AudioFormatManager& audioFormatManager;
	juce::UndoManager undoManager;
	juce::ListenerList<Listener> listenrList;
	juce::File lastFile;
	WaveFormat waveFormat = {};
	int sourceBitsPerSample = 0;
	juce::StringPairArray sourceMetaData;
	WaveCutList waveCutList;
	int64_t totalLength = 0;
	juce::SharedResourcePointer<WaveCutListClipboard> clipboard;
	WaveCutListDocumentImpl(juce::AudioFormatManager& afm) : WaveCutListDocument(".wav", "*.wav", "Choose a file to open", "Choose a file to save as"), audioFormatManager(afm)
	{
	}
	virtual ~WaveCutListDocumentImpl()
	{
	}
	bool isArchiveBasedCutList() const
	{
		return (waveCutList.size() == 1) && (dynamic_cast<ArchivedWaveSourceFile*>(waveCutList.front().sourceFile.get()) != nullptr);
	}
	void switchToTempBasedCutList()
	{
		if(!isArchiveBasedCutList()) return;
		WaveSourceFile::Ptr tmpsrcfile = TemporaryWaveSourceFile::createInstanceFromSourceFile(waveCutList.front().sourceFile);
		if(!tmpsrcfile) return;
		WaveSourceFile::Ptr arcsrcfile = waveCutList.front().sourceFile;
		waveCutList.front().sourceFile = tmpsrcfile;
		listenrList.call(&Listener::waveCutListDocumentDidReplaceSourceFile, this, arcsrcfile, tmpsrcfile);
	}
	void clearContents()
	{
		undoManager.clearUndoHistory();
		waveFormat = {};
		sourceBitsPerSample = 0;
		sourceMetaData.clear();
		waveCutList.clear();
		totalLength = 0;
	}
	virtual juce::String getDocumentTitle() override
	{
		return lastFile == juce::File() ? juce::String("untitled") : lastFile.getFileNameWithoutExtension();
	}
	virtual juce::Result loadDocument(const juce::File& path) override
	{
		juce::Result r = juce::Result::fail("unexpected");
		try
		{
			clearContents();
			std::unique_ptr<juce::AudioFormatReader> reader(audioFormatManager.createReaderFor(path));
			if(!reader) throw juce::Result::fail("failed to open");
			sourceBitsPerSample = reader->bitsPerSample;
			sourceMetaData = reader->metadataValues;
			WaveSourceFile::Ptr srcfile = ArchivedWaveSourceFile::createInstance(std::move(reader), path);
			if(!srcfile) throw juce::Result::fail("failed to open");
			waveFormat = srcfile->format;
			waveCutList.push_back({ srcfile, { 0, srcfile->length } });
			totalLength = waveCutList.calcTotalSize();
			DBG("[WaveCutListDocument] loadDocument() totalLength=" << totalLength);
			r = juce::Result::ok();
		}
		catch(juce::Result& e)
		{
			r = e;
			clearContents();
			DBG("[WaveCutListDocument] loadDocument() " << e.getErrorMessage().quoted());
		}
		listenrList.call(&Listener::waveCutListDocumentDidInit, this);
		sendChangeMessage();
		return r;
	}
	virtual juce::Result saveDocument(const juce::File& path) override
	{
		// TODO: asynchronous processing
		juce::Result r = juce::Result::fail("unexpected");
		try
		{
			if(isArchiveBasedCutList() && (waveCutList.front().sourceFile->backingFile == path))
			{
				switchToTempBasedCutList();
			}
			// open
			juce::AudioFormat* af = audioFormatManager.findFormatForFileExtension(path.getFileExtension());
			if(!af) throw juce::Result::fail("format not found");
			if(path.exists()) path.deleteFile();
			std::unique_ptr<juce::FileOutputStream> ostr(new juce::FileOutputStream(path));
			if(ostr->failedToOpen()) throw juce::Result::fail("failed to open");
			ostr->setPosition(0);
			ostr->truncate();
			std::unique_ptr<juce::AudioFormatWriter> writer(af->createWriterFor(ostr.get(), waveFormat.sampleRate, waveFormat.numChannels, sourceBitsPerSample, sourceMetaData, 0));
			if(!writer) throw juce::Result::fail("failed to create a writer");
			ostr.release();
			// transfer
			juce::AudioSampleBuffer buffer(waveFormat.numChannels, 16384);
			WaveCutListReader::Ptr reader = WaveCutListReader::createInstance();
			reader->setWaveCutList(waveCutList);
			reader->setPosition(0);
			int64_t len = totalLength, pos = 0;
			while(pos < len)
			{
				int lseg = (int)std::min((int64_t)buffer.getNumSamples(), len - pos);
				reader->read(buffer.getArrayOfWritePointers(), waveFormat.numChannels, lseg);
				writer->writeFromAudioSampleBuffer(buffer, 0, lseg);
				pos += lseg;
			}
			r = juce::Result::ok();
		}
		catch(juce::Result& e)
		{
			r = e;
			DBG("[WaveCutListDocument] saveDocument() " << e.getErrorMessage().quoted());
		}
		return r;
	}
	virtual juce::File getLastDocumentOpened() override
	{
		return lastFile;
	}
	virtual void setLastDocumentOpened(const juce::File& file) override
	{
		lastFile = file;
	}
	// --------------------------------------------------------------------------------
	virtual void addListener(Listener* v) override
	{
		listenrList.add(v);
	}
	virtual void removeListener(Listener* v) override
	{
		listenrList.remove(v);
	}
	virtual bool hasValidContent() const override
	{
		return (0 < waveFormat.sampleRate) && (0 < waveFormat.numChannels);
	}
	virtual WaveFormat getWaveFormat() const override
	{
		return waveFormat;
	}
	virtual const WaveCutList& getWaveCutlist() const override
	{
		return waveCutList;
	}
	virtual int64_t getTotalLength() const override
	{
		return totalLength;
	}
	virtual bool canUndo() const override
	{
		return hasValidContent() && undoManager.canUndo();
	}
	virtual bool canRedo() const override
	{
		return hasValidContent() && undoManager.canRedo();
	}
	virtual bool canErase(const Range64& r) const override
	{
		return hasValidContent() && !r.isEmpty() && r.intersects({ 0, totalLength });
	}
	virtual bool canCut(const Range64& r) const override
	{
		return hasValidContent() && !r.isEmpty() && r.intersects({ 0, totalLength });
	}
	virtual bool canCopy(const Range64& r) const override
	{
		return hasValidContent() && !r.isEmpty() && r.intersects({ 0, totalLength });
	}
	virtual bool canPaste(int64_t t) const override
	{
		return hasValidContent() && (0 <= t) && (t <= totalLength) && !clipboard->isEmpty() && (waveFormat == clipboard->getFormat());
	}
	virtual bool canFadein(const Range64& r) const override
	{
		return hasValidContent() && !r.isEmpty() && r.intersects({ 0, totalLength });
	}
	virtual bool canFadeout(const Range64& r) const override
	{
		return hasValidContent() && !r.isEmpty() && r.intersects({ 0, totalLength });
	}
	// --------------------------------------------------------------------------------
	virtual bool undo() override
	{
		if(!canUndo()) return false;
		switchToTempBasedCutList();
		if(!undoManager.undo()) return false;
		waveCutList.mergeAdjucentContinuousCuts();
		totalLength = waveCutList.calcTotalSize();
		listenrList.call(&Listener::waveCutListDocumentDidEdit, this, EditUnknown, Range64{ 0, totalLength });
		changed();
		DBG("[WaveCutListDocument] edit-undo: cutlistsize=" << (int)waveCutList.size() << " totallength=" << totalLength);
		return true;
	}
	virtual bool redo() override
	{
		if(!canRedo()) return false;
		switchToTempBasedCutList();
		if(!undoManager.redo()) return false;
		waveCutList.mergeAdjucentContinuousCuts();
		totalLength = waveCutList.calcTotalSize();
		listenrList.call(&Listener::waveCutListDocumentDidEdit, this, EditUnknown, Range64{ 0, totalLength });
		changed();
		DBG("[WaveCutListDocument] edit-redo: cutlistsize=" << (int)waveCutList.size() << " totallength=" << totalLength);
		return true;
	}
	virtual bool erase(const Range64& r) override
	{
		if(!canErase(r)) return false;
		switchToTempBasedCutList();
		ScopedUndoTransaction sut(undoManager, "erase");
		if(!undoManager.perform(new WaveEraseUndoAction(waveCutList, r))) return false;
		waveCutList.mergeAdjucentContinuousCuts();
		totalLength = waveCutList.calcTotalSize();
		listenrList.call(&Listener::waveCutListDocumentDidEdit, this, EditErase, r);
		changed();
		DBG("[WaveCutListDocument] edit-erase: cutlistsize=" << (int)waveCutList.size() << " totallength=" << totalLength);
		return true;
	}
	virtual bool cut(const Range64& r) override
	{
		if(!canCut(r)) return false;
		switchToTempBasedCutList();
		clipboard->setCutList(waveCutList.intersectRange(r));
		ScopedUndoTransaction sut(undoManager, "cut");
		if(!undoManager.perform(new WaveEraseUndoAction(waveCutList, r))) return false;
		waveCutList.mergeAdjucentContinuousCuts();
		totalLength = waveCutList.calcTotalSize();
		listenrList.call(&Listener::waveCutListDocumentDidEdit, this, EditErase, r);
		changed();
		DBG("[WaveCutListDocument] edit-cut: cutlistsize=" << (int)waveCutList.size() << " totallength=" << totalLength);
		return true;
	}
	virtual bool copy(const Range64& r) override
	{
		if(!canCopy(r)) return false;
		switchToTempBasedCutList();
		clipboard->setCutList(waveCutList.intersectRange(r));
		return true;
	}
	virtual bool paste(int64_t t) override
	{
		if(!canPaste(t)) return false;
		switchToTempBasedCutList();
		ScopedUndoTransaction sut(undoManager, "paste");
		const WaveCutList& clins = clipboard->getCutList();
		if(!undoManager.perform(new WaveInsertUndoAction(waveCutList, clins, t))) return false;
		waveCutList.mergeAdjucentContinuousCuts();
		totalLength = waveCutList.calcTotalSize();
		listenrList.call(&Listener::waveCutListDocumentDidEdit, this, EditInsert, Range64{ t, t + clins.calcTotalSize() });
		changed();
		DBG("[WaveCutListDocument] edit-paste: cutlistsize=" << (int)waveCutList.size() << " totallength=" << totalLength);
		return true;
	}
	virtual bool fadein(const Range64& r) override
	{
		if(!canFadein(r)) return false;
		switchToTempBasedCutList();
		WaveCutList clramp = WaveCutListModifier::processSyncWithRamp(waveCutList, r, 0, 1);
		ScopedUndoTransaction sut(undoManager, "fadein");
		if(!undoManager.perform(new WaveEraseUndoAction(waveCutList, r))) return false;
		if(!undoManager.perform(new WaveInsertUndoAction(waveCutList, clramp, r.begin))) return false;
		jassert(totalLength == waveCutList.calcTotalSize());
		listenrList.call(&Listener::waveCutListDocumentDidEdit, this, EditReplace, r);
		changed();
		DBG("[WaveCutListDocument] edit-fadein: cutlistsize=" << (int)waveCutList.size() << " totallength=" << totalLength);
		return false;
	}
	virtual bool fadeout(const Range64& r) override
	{
		if(!canFadeout(r)) return false;
		switchToTempBasedCutList();
		WaveCutList clramp = WaveCutListModifier::processSyncWithRamp(waveCutList, r, 1, 0);
		ScopedUndoTransaction sut(undoManager, "fadeout");
		if(!undoManager.perform(new WaveEraseUndoAction(waveCutList, r))) return false;
		if(!undoManager.perform(new WaveInsertUndoAction(waveCutList, clramp, r.begin))) return false;
		jassert(totalLength == waveCutList.calcTotalSize());
		listenrList.call(&Listener::waveCutListDocumentDidEdit, this, EditReplace, r);
		changed();
		DBG("[WaveCutListDocument] edit-fadeout: cutlistsize=" << (int)waveCutList.size() << " totallength=" << totalLength);
		return false;
	}
};

WaveCutListDocument* WaveCutListDocument::createInstance(juce::AudioFormatManager& afm)
{
	return new WaveCutListDocumentImpl(afm);
}
