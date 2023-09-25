//
//  WaveCutListPlayer.h
//  TestWaveEdit_App
//
//  created by yu2924 on 2023-03-25
//

#pragma once

#include "WaveCutListDocument.h"

class WaveCutListPlayer : public juce::ChangeBroadcaster
{
public:
	virtual double getDuration() const = 0;
	virtual bool isLooping() const = 0;
	virtual void setLooping(bool v) = 0;
	virtual double getPosition() const = 0;
	virtual void setPosition(double v) = 0;
	virtual bool isRunning() const = 0;
	virtual void setRunning(bool v) = 0;
	static std::unique_ptr<WaveCutListPlayer> createInstance(juce::AudioDeviceManager& adm, WaveCutListDocument& doc);
};
