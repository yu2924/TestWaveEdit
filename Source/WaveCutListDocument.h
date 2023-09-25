//
//  WaveCutListDocument.h
//  TestWaveEdit_App
//
//  created by yu2924 on 2023-03-25
//

#pragma once

#include <JuceHeader.h>
#include "WaveCutList.h"

class WaveCutListDocument : public juce::FileBasedDocument
{
protected:
	WaveCutListDocument(const juce::String& ext, const juce::String& wildcard, const juce::String& opentitle, const juce::String& savetitle) : juce::FileBasedDocument(ext, wildcard, opentitle, savetitle) {}
public:
	enum EditType
	{
		EditUnknown,
		EditInsert,
		EditErase,
		EditReplace,
	};
	struct Listener
	{
		virtual ~Listener() {}
		virtual void waveCutListDocumentDidInit(WaveCutListDocument*) = 0;
		virtual void waveCutListDocumentDidReplaceSourceFile(WaveCutListDocument*, WaveSourceFile::Ptr prevsrcfile, WaveSourceFile::Ptr newsrcfile) = 0;
		virtual void waveCutListDocumentDidEdit(WaveCutListDocument*, int edittype, const Range64& r) = 0;
	};
	virtual void addListener(Listener*) = 0;
	virtual void removeListener(Listener*) = 0;
	virtual bool hasValidContent() const = 0;
	virtual WaveFormat getWaveFormat() const = 0;
	virtual const WaveCutList& getWaveCutlist() const = 0;
	virtual int64_t getTotalLength() const = 0;
	virtual bool canUndo() const = 0;
	virtual bool canRedo() const = 0;
	virtual bool canErase(const Range64& r) const = 0;
	virtual bool canCut(const Range64& r) const = 0;
	virtual bool canCopy(const Range64& r) const = 0;
	virtual bool canPaste(int64_t t) const = 0;
	virtual bool canFadein(const Range64& r) const = 0;
	virtual bool canFadeout(const Range64& r) const = 0;
	virtual bool undo() = 0;
	virtual bool redo() = 0;
	virtual bool erase(const Range64& r) = 0;
	virtual bool cut(const Range64& r) = 0;
	virtual bool copy(const Range64& r) = 0;
	virtual bool paste(int64_t t) = 0;
	virtual bool fadein(const Range64& r) = 0;
	virtual bool fadeout(const Range64& r) = 0;
	static WaveCutListDocument* createInstance(juce::AudioFormatManager& afm);
};
