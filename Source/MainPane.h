//
//  MainPane.h
//  TestWaveEdit_App
//
//  created by yu2924 on 2023-09-27
//

#pragma once

#include <JuceHeader.h>
#include "WaveCutListDocument.h"
#include "WaveCutListPlayer.h"

class MainPane : public juce::Component
{
protected:
	class Impl;
	std::unique_ptr<Impl> impl;
public:
	MainPane(juce::ApplicationCommandManager& acm, juce::AudioFormatManager& afm, WaveCutListDocument& doc, WaveCutListPlayer& play);
	virtual ~MainPane();
	virtual void resized() override;
	virtual void paint(juce::Graphics& g) override;
	int64_t getCursorPosition64() const;
	Range64 getSelectionRange64() const;
};
