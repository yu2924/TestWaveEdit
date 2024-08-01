//
//  WaveCutListView.cpp
//  TestWaveEdit_App
//
//  created by yu2924 on 2023-03-25
//

#include "WaveCutListView.h"

class WaveCutListView::PlotPane : public juce::Component, public juce::ChangeListener
{
public:
	const juce::Colour CursorColor{ 0xffff7f0e }; // TAB10:orange
	const juce::Colour WaveformColor{ 0xff20a685 };
	const juce::Colour BackgroundColor{ 0xff202020 };
	juce::AudioFormatManager& audioFormatManager;
	juce::AudioThumbnailCache audioThumbnailCache;
	struct WaveSourceFileComparator
	{
		bool operator()(const WaveSourceFile::Ptr& a, const WaveSourceFile::Ptr& b) const { return a.get() < b.get(); }
	};
	std::map<WaveSourceFile::Ptr, std::unique_ptr<juce::AudioThumbnail>, WaveSourceFileComparator> thumbnailMap;
	WaveFormat waveFormat = {};
	WaveCutList waveCutList;
	int64_t totallength = 0;
	double duration = 0;
	juce::Range<double> selectionRange = {};
	double cursorPosition = 0;
	double zoomFactor = 1;
	class Cursor : public juce::Component
	{
	public:
		juce::Colour cursorColor;
		Cursor() { setOpaque(true); }
		virtual void paint(juce::Graphics& g) override { g.fillAll(cursorColor); }
		void setCursorColor(const juce::Colour& v) { cursorColor = v; repaint(); }
	} cursor;
	struct
	{
		int xstart;
		bool dragged;
	} dragCtx = {};
	static constexpr int XMargin = 8;
	PlotPane(juce::AudioFormatManager& afm) : audioFormatManager(afm), audioThumbnailCache(16)
	{
		setOpaque(true);
		addAndMakeVisible(cursor);
		cursor.setCursorColor(CursorColor);
	}
	~PlotPane()
	{
		for(auto&& ent : thumbnailMap) ent.second->removeChangeListener(this);
	}
	WaveCutListView* getParentView() const
	{
		return findParentComponentOfClass<WaveCutListView>();
	}
	double x2t(int x)
	{
		if(getWidth() <= (XMargin * 2)) return 0;
		return duration * (double)(x - XMargin) / (double)(getWidth() - (XMargin * 2));
	}
	int t2x(double t)
	{
		if(duration <= 0) return 0;
		return XMargin + juce::roundToInt((double)(getWidth() - (XMargin * 2)) * t / duration);
	}
	int s2x(int64_t i)
	{
		return XMargin + juce::roundToInt((getWidth() - (XMargin * 2)) * (double)i / (double)totallength);
	}
	void updateSelectionRange(int xa, int xb)
	{
		selectionRange = {};
		double ta = x2t(xa);
		double tb = x2t(xb);
		double begin = std::max(0.0, std::min(ta, tb));
		double end = std::min(duration, std::max(ta, tb));
		selectionRange = { begin, end };
		if(getParentView()->onSelectionRangeChange) getParentView()->onSelectionRangeChange(selectionRange);
		repaint();
	}
	juce::Rectangle<int> calcCursorPosition(double t)
	{
		return { t2x(t), 0, 1, getHeight() };
	}
	void updateCursorPosition()
	{
		cursor.setBounds(calcCursorPosition(cursorPosition));
	}
	void fireClick(int x)
	{
		WaveCutListView* parentvp = getParentView();
		if(!parentvp) return;
		if(parentvp->onClick) parentvp->onClick(std::max(0.0, std::min(duration, x2t(x))));
	}
	void resizeAccordingToZoomFactor()
	{
		WaveCutListView* parentvp = getParentView();
		if(!parentvp) return;
		setSize(juce::roundToInt(parentvp->getWidth() * zoomFactor), parentvp->getHeight() - parentvp->getScrollBarThickness());
	}
	void ensureTimeVisibleByDrag(double tfocus)
	{
		WaveCutListView* parentvp = getParentView();
		if(!parentvp) return;
		if(duration <= 0) return;
		juce::Rectangle rcv = parentvp->getViewArea().reduced(XMargin);
		juce::Point<int> vpos = parentvp->getViewPosition();
		int xfocus = t2x(tfocus);
		if(rcv.getRight() <= xfocus) parentvp->setViewPosition(xfocus - rcv.getWidth() - XMargin, vpos.y);
		else if(xfocus < rcv.getX()) parentvp->setViewPosition(xfocus - XMargin, vpos.y);
	}
	void ensureTimeVisibleByPlayback(double tfocus)
	{
		WaveCutListView* parentvp = getParentView();
		if(!parentvp) return;
		if(duration <= 0) return;
		juce::Rectangle rcv = parentvp->getViewArea();
		juce::Point<int> vpos = parentvp->getViewPosition();
		int xfocus = t2x(tfocus);
		int cxv = rcv.getWidth();
		int cxm = cxv / 8;
		if((xfocus < vpos.x) || ((vpos.x + cxv - cxm) <= xfocus)) parentvp->setViewPosition(xfocus - XMargin, vpos.y);
	}
	// --------------------------------------------------------------------------------
	// juce::Component
	virtual void mouseWheelMove(const juce::MouseEvent& me, const juce::MouseWheelDetails& mwd) override
	{
		if(me.mods.isCommandDown() && (0 < duration))
		{
			double tanchor = x2t(me.x);
			double newzoomfactor = zoomFactor * pow(1.1, mwd.deltaY);
			setZoomFactor(newzoomfactor, tanchor, true);
		}
		else juce::Component::mouseWheelMove(me, mwd);
	}
	virtual void resized() override
	{
		updateCursorPosition();
	}
	virtual void paint(juce::Graphics& g) override
	{
		juce::Rectangle<int> rc = getLocalBounds();
		juce::Rectangle<int> rcclip = g.getClipBounds();
		g.setColour(BackgroundColor);
		g.fillRect(rcclip);
		if(duration <= 0) return;
		double fs = waveFormat.sampleRate;
		if(!waveCutList.empty())
		{
			g.setColour(WaveformColor);
			int64_t spos = 0;
			for(const auto& wc : waveCutList)
			{
				int xl = s2x(spos);
				int xr = s2x(spos + wc.range.size());
				juce::Rectangle<int> rcseg(xl, rc.getY(), xr - xl, rc.getHeight());
				if(rcseg.intersects(rcclip))
				{
					auto it = thumbnailMap.find(wc.sourceFile);
					if(it != thumbnailMap.end())
					{
						juce::AudioThumbnail* th = it->second.get();
						double tl = (double)wc.range.begin / fs;
						double tr = (double)wc.range.end / fs;
						th->drawChannels(g, rcseg, tl, tr, 1);
					}
				}
				spos += wc.range.size();
			}
		}
		if(!selectionRange.isEmpty())
		{
			int xl = t2x(selectionRange.getStart());
			int xr = t2x(selectionRange.getEnd());
			g.setColour(juce::Colour(0x40ffffff));
			g.fillRect(juce::Rectangle<int>(xl, rc.getY(), xr - xl, rc.getHeight()));
		}
	}
	virtual void mouseDown(const juce::MouseEvent& me) override
	{
		dragCtx = { me.x, false };
		fireClick(me.x);
		updateSelectionRange(dragCtx.xstart, me.x);
	}
	virtual void mouseDrag(const juce::MouseEvent& me) override
	{
		if(3 <= std::abs(me.x - dragCtx.xstart)) dragCtx.dragged = true;
		if(dragCtx.dragged)
		{
			updateSelectionRange(dragCtx.xstart, me.x);
			ensureTimeVisibleByDrag(x2t(me.x));
		}
	}
	virtual void mouseUp(const juce::MouseEvent& me) override
	{
		if(dragCtx.dragged) fireClick(std::min(dragCtx.xstart, me.x));
	}
	// --------------------------------------------------------------------------------
	// juce::ChangeListener
	virtual void changeListenerCallback(juce::ChangeBroadcaster* source) override
	{
		if(dynamic_cast<juce::AudioThumbnail*>(source)) repaint();
	}
	// --------------------------------------------------------------------------------
	// APIs
	void replaceSourceFile(WaveSourceFile::Ptr prevsrcfile, WaveSourceFile::Ptr newsrcfile)
	{
		auto itfrom = thumbnailMap.find(prevsrcfile);
		if(itfrom == thumbnailMap.end()) return;
		juce::MemoryBlock mb;
		{
			juce::MemoryOutputStream ostr;
			itfrom->second->saveTo(ostr);
			mb = ostr.getMemoryBlock();
		}
		std::unique_ptr<juce::AudioThumbnail> th = std::make_unique<juce::AudioThumbnail>(128, audioFormatManager, audioThumbnailCache);
		{
			juce::MemoryInputStream istr(std::move(mb));
			th->loadFrom(istr);
		}
		// th->setSource(new juce::FileInputSource(newsrcfile->backingFile)); // is this necessary?
		th->addChangeListener(this);
		thumbnailMap[newsrcfile].reset(th.release());
		thumbnailMap.erase(itfrom);
	}
	void setContent(const WaveFormat& fmt, const WaveCutList& cl, bool init)
	{
		if(init)
		{
			for(auto&& ent : thumbnailMap) ent.second->removeChangeListener(this);
			thumbnailMap.clear();
		}
		waveFormat = fmt;
		waveCutList = cl;
		totallength = cl.calcTotalSize();
		duration = (0 < waveFormat.sampleRate) ? ((double)totallength / waveFormat.sampleRate) : 0;
		for(const auto& wc : waveCutList)
		{
			auto it = thumbnailMap.find(wc.sourceFile);
			if(it == thumbnailMap.end())
			{
				std::unique_ptr<juce::AudioThumbnail> th = std::make_unique<juce::AudioThumbnail>(128, audioFormatManager, audioThumbnailCache);
				th->setSource(new juce::FileInputSource(wc.sourceFile->backingFile));
				th->addChangeListener(this);
				thumbnailMap[wc.sourceFile].reset(th.release());
			}
		}
	}
	const juce::Range<double> getSelectionRange() const
	{
		return selectionRange;
	}
	const void setSelectionRange(const juce::Range<double>& v)
	{
		double va = v.getStart();
		double vb = v.getEnd();
		double begin = std::max(0.0, std::min(va, vb));
		double end   = std::min(duration, std::max(va, vb));
		selectionRange = { begin, end };
		repaint();
	}
	double getCursorPosition() const
	{
		return cursorPosition;
	}
	void setCursorPosition(double v, bool ensurevisible, bool running)
	{
		cursorPosition = std::max(0.0, std::min(duration, v));
		updateCursorPosition();
		juce::ComponentAnimator& anim = juce::Desktop::getInstance().getAnimator();
		anim.cancelAnimation(&cursor, false);
		if(running)
		{
			if(ensurevisible) ensureTimeVisibleByPlayback(cursorPosition);
			juce::Rectangle<int> rce = calcCursorPosition(duration);
			int tms = juce::roundToInt((duration - cursorPosition) * 1000);
			anim.animateComponent(&cursor, rce, 1, tms, false, 1, 1);
		}
		else if(ensurevisible) ensureTimeVisibleByDrag(cursorPosition);
	}
	double getZoomFactor() const
	{
		return zoomFactor;
	}
	void setZoomFactor(double zfact, double tanchor, bool anchorcentric)
	{
		// NOTE:
		// the maximum width of a component that can be correctly rendered seems to be:
		// * 0x02000000 on Win32
		// * 0x01000000 on macOS
		int xanchor = t2x(tanchor) + getX();
		constexpr int MAXCOORDS = 0x01000000;
		double maxzoom = (double)MAXCOORDS / (double)getWidth();
		zoomFactor = std::max(1.0, std::min(maxzoom, zfact));
		resizeAccordingToZoomFactor();
		if(anchorcentric)
		{
			int xoff = t2x(tanchor) - xanchor;
			getParentView()->setViewPosition(xoff, -getY());
		}
	}
};

WaveCutListView::WaveCutListView(juce::AudioFormatManager& afm)
{
	setScrollBarsShown(false, true);
	PlotPane* pane = new PlotPane(afm);
	setViewedComponent(pane, true);
}

WaveCutListView::~WaveCutListView()
{
}

WaveCutListView::PlotPane* WaveCutListView::getPlotPane() const
{
	return dynamic_cast<PlotPane*>(getViewedComponent());
}

void WaveCutListView::resized()
{
	Viewport::resized();
	getPlotPane()->resizeAccordingToZoomFactor();
}

void WaveCutListView::setContent(const WaveFormat& fmt, const WaveCutList& cl, bool reset) { getPlotPane()->setContent(fmt, cl, reset); }
const juce::Range<double> WaveCutListView::getSelectionRange() const { return getPlotPane()->getSelectionRange(); }
void WaveCutListView::replaceSourceFile(WaveSourceFile::Ptr prevsrcfile, WaveSourceFile::Ptr newsrcfile) { getPlotPane()->replaceSourceFile(prevsrcfile, newsrcfile); }
const void WaveCutListView::setSelectionRange(const juce::Range<double>& v) { getPlotPane()->setSelectionRange(v); }
double WaveCutListView::getCursorPosition() const { return getPlotPane()->getCursorPosition(); }
void WaveCutListView::setCursorPosition(double v, bool ensurevisible, bool running) { getPlotPane()->setCursorPosition(v, ensurevisible, running); }
double WaveCutListView::getZoomFactor() const { return getPlotPane()->getZoomFactor(); }
void WaveCutListView::setZoomFactor(double zfact, double tanchor, bool anchorcentric) { getPlotPane()->setZoomFactor(zfact, tanchor, anchorcentric); }
