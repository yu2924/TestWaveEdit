//
//  WaveCutListView.h
//  TestWaveEdit_App
//
//  created by yu2924 on 2023-03-25
//

#pragma once

#include "WaveCutList.h"

class WaveCutListView : public juce::Viewport
{
protected:
	class PlotPane;
	PlotPane* getPlotPane() const;
	double zoomFactor = 1;
public:
	std::function<void(double)> onClick;
	std::function<void(const juce::Range<double>&)> onSelectionRangeChange;
	WaveCutListView(juce::AudioFormatManager& afm);
	virtual ~WaveCutListView();
	virtual void resized() override;
	void replaceSourceFile(WaveSourceFile::Ptr prevsrcfile, WaveSourceFile::Ptr newsrcfile);
	void setContent(const WaveFormat& fmt, const WaveCutList& cl, bool init);
	const juce::Range<double> getSelectionRange() const;
	const void setSelectionRange(const juce::Range<double>& v);
	double getCursorPosition() const;
	void setCursorPosition(double v, bool ensurevisible, bool running);
	double getZoomFactor() const;
	void setZoomFactor(double zfact, double tanchor, bool anchorcentric);
};
