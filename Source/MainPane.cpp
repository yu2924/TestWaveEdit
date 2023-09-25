//
//  MainPane.cpp
//  TestWaveEdit_App
//
//  created by yu2924 on 2023-09-27
//

#include "MainPane.h"
#include "WaveCutListView.h"
#include "CommandIDs.h"

static const char SvgTransportRun[] =
	R"(<svg xmlns="http://www.w3.org/2000/svg">)"
	R"(<path d="m6.35 3.9687-4.2333-1.8521v3.7042z" fill="none" stroke="#000" stroke-linecap="round" stroke-linejoin="round" stroke-width=".52917"/>)"
	R"(</svg>)";
static const char SvgTransportLoop[] =
	R"(<svg xmlns="http://www.w3.org/2000/svg">)"
	R"(<g fill="none" stroke="#000" stroke-linecap="round" stroke-linejoin="round">)"
	R"(<path d="m2.5216 6.1837a2.6458 2.6458 0 0 1-1.1986-2.215 2.6458 2.6458 0 0 1 2.6458-2.6458 2.6458 2.6458 0 0 1 2.6458 2.6458 2.6458 2.6458 0 0 1-1.212 2.2236" color="#000000" stroke-width=".52917"/>)"
	R"(<path d="m5.2917 5.0271 0.11092 1.1653 1.212-0.10697" stroke-width=".52917"/>)"
	R"(</g>)"
	R"(</svg>)";

class MainPane::Impl : public juce::Timer, public WaveCutListDocument::Listener, public juce::ChangeListener
{
public:
	std::unique_ptr<juce::Drawable> loadSvgAsDrawable(const char* svgsz, juce::Colour replacedcolour)
	{
		if(!svgsz) return nullptr;
		std::unique_ptr<juce::Drawable> img = juce::Drawable::createFromImageData(svgsz, juce::CharPointer_ASCII(svgsz).length());
		img->replaceColour(replacedcolour, juce::LookAndFeel::getDefaultLookAndFeel().findColour(juce::Label::ColourIds::textColourId));
		return img;
	}
	MainPane& owner;
	juce::ApplicationCommandManager& applicationCommandManager;
	WaveCutListDocument& document;
	WaveCutListPlayer& player;
	WaveCutListView view;
	juce::DrawableButton runButton;
	juce::DrawableButton loopButton;
	juce::Label posLabel;
	juce::Label posEdit;
	juce::Label selRangeLabel;
	juce::Label selBeginEdit;
	juce::Label selEndEdit;
	enum { Margin = 4, Spacing = 4, BarHeight = 32, ButtonWidth = 32, EditWidth = 64, };
	Impl(MainPane& o, juce::ApplicationCommandManager& acm, juce::AudioFormatManager& afm, WaveCutListDocument& doc, WaveCutListPlayer& play)
		: owner(o)
		, applicationCommandManager(acm)
		, document(doc)
		, player(play)
		, view(afm)
		, runButton("run", juce::DrawableButton::ButtonStyle::ImageOnButtonBackground)
		, loopButton("loop", juce::DrawableButton::ButtonStyle::ImageOnButtonBackground)
	{
	}
	void construct()
	{
		juce::LookAndFeel& lf = owner.getLookAndFeel();
		// view
		owner.addAndMakeVisible(view);
		view.onClick = [this](double v) { onWvClick(v); };
		view.onSelectionRangeChange = [this](const juce::Range<double>& v) { onWvSelRangeChange(v); };
		// run
		owner.addAndMakeVisible(runButton);
		runButton.setImages(loadSvgAsDrawable(SvgTransportRun, juce::Colours::black).get(), nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
		runButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff0080ff));
		runButton.setCommandToTrigger(&applicationCommandManager, CommandIDs::TransportRun, true);
		// loop
		owner.addAndMakeVisible(loopButton);
		loopButton.setImages(loadSvgAsDrawable(SvgTransportLoop, juce::Colours::black).get(), nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
		loopButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff0080ff));
		loopButton.setCommandToTrigger(&applicationCommandManager, CommandIDs::TransportLoop, true);
		// pos
		owner.addAndMakeVisible(posLabel);
		posLabel.setJustificationType(juce::Justification::centredRight);
		posLabel.setText("position", juce::dontSendNotification);
		owner.addAndMakeVisible(posEdit);
		posEdit.setEditable(true);
		posEdit.setColour(juce::Label::ColourIds::outlineColourId, lf.findColour(juce::TextEditor::ColourIds::outlineColourId));
		posEdit.setColour(juce::Label::ColourIds::backgroundColourId, lf.findColour(juce::TextEditor::ColourIds::backgroundColourId));
		posEdit.onTextChange = [this]() { onPosEditChange(); };
		// selrange
		owner.addAndMakeVisible(selRangeLabel);
		selRangeLabel.setJustificationType(juce::Justification::centredRight);
		selRangeLabel.setText("selection", juce::dontSendNotification);
		owner.addAndMakeVisible(selBeginEdit);
		selBeginEdit.setEditable(true);
		selBeginEdit.setColour(juce::Label::ColourIds::outlineColourId, lf.findColour(juce::TextEditor::ColourIds::outlineColourId));
		selBeginEdit.setColour(juce::Label::ColourIds::backgroundColourId, lf.findColour(juce::TextEditor::ColourIds::backgroundColourId));
		selBeginEdit.onTextChange = [this]() { onSelEditChange(); };
		owner.addAndMakeVisible(selEndEdit);
		selEndEdit.setEditable(true);
		selEndEdit.setColour(juce::Label::ColourIds::outlineColourId, lf.findColour(juce::TextEditor::ColourIds::outlineColourId));
		selEndEdit.setColour(juce::Label::ColourIds::backgroundColourId, lf.findColour(juce::TextEditor::ColourIds::backgroundColourId));
		selEndEdit.onTextChange = [this]() { onSelEditChange(); };
		// init
		updateSelection();
		updateCursorPosition(false);
		document.addListener(this);
		player.addChangeListener(this);
	}
	~Impl()
	{
		player.removeChangeListener(this);
		document.removeListener(this);
	}
	// --------------------------------------------------------------------------------
	// internal
	void updateSelection()
	{
		double duration = (double)document.getTotalLength() / document.getWaveFormat().sampleRate;
		juce::Range<double> sel = view.getSelectionRange();
		double end = std::max(0.0, std::min(duration, sel.getEnd()));
		double start = std::max(0.0, std::min(std::min(duration, end), sel.getStart()));
		sel = { start, end };
		selBeginEdit.setText(juce::String::formatted("%.3f", sel.getStart()), juce::dontSendNotification);
		selEndEdit.setText(juce::String::formatted("%.3f", sel.getEnd()), juce::dontSendNotification);
		view.setSelectionRange(sel);
		applicationCommandManager.commandStatusChanged();
	}
	void updateCursorPosition(bool ensurevisible)
	{
		double duration = (double)document.getTotalLength() / document.getWaveFormat().sampleRate;
		double t = player.getPosition();
		t = std::max(0.0, std::min(duration, t));
		posEdit.setText(juce::String::formatted("%.3f", t), juce::dontSendNotification);
		view.setCursorPosition(t, ensurevisible, player.isRunning());
	}
	void onPosEditChange()
	{
		double t = posEdit.getText().getDoubleValue();
		player.setPosition(t);
		updateCursorPosition(true);
	}
	void onSelEditChange()
	{
		juce::Range<double> sel = { selBeginEdit.getText().getDoubleValue(), selEndEdit.getText().getDoubleValue() };
		view.setSelectionRange(sel);
		updateSelection();
	}
	void onWvClick(double v)
	{
		player.setPosition(v);
		updateCursorPosition(true);
	}
	void onWvSelRangeChange(const juce::Range<double>&)
	{
		updateSelection();
	}
	// --------------------------------------------------------------------------------
	// juce::Component
	void resized()
	{
		juce::Rectangle<int> rc = owner.getLocalBounds();
		juce::Rectangle<int> rcbar = rc.removeFromBottom(BarHeight).reduced(Margin);
		runButton.setBounds(rcbar.removeFromLeft(ButtonWidth));
		rcbar.removeFromLeft(Spacing);
		loopButton.setBounds(rcbar.removeFromLeft(ButtonWidth));
		rcbar.removeFromLeft(Spacing);
		selEndEdit.setBounds(rcbar.removeFromRight(EditWidth));
		rcbar.removeFromRight(Spacing);
		selBeginEdit.setBounds(rcbar.removeFromRight(EditWidth));
		selRangeLabel.setBounds(rcbar.removeFromRight(selRangeLabel.getFont().getStringWidth(selRangeLabel.getText()) + Spacing));
		rcbar.removeFromRight(Spacing);
		posEdit.setBounds(rcbar.removeFromRight(EditWidth));
		posLabel.setBounds(rcbar.removeFromRight(posLabel.getFont().getStringWidth(posLabel.getText()) + Spacing));
		view.setBounds(rc.reduced(Margin, 0));
	}
	void paint(juce::Graphics& g)
	{
		g.fillAll(owner.getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
	}
	// --------------------------------------------------------------------------------
	// juce::Timer
	virtual void timerCallback() override
	{
		updateCursorPosition(true);
	}
	// --------------------------------------------------------------------------------
	// WaveCutListDocument::Listener
	virtual void waveCutListDocumentDidInit(WaveCutListDocument*) override
	{
		view.setContent(document.getWaveFormat(), document.getWaveCutlist(), true);
		view.setSelectionRange({ 0, 0 });
		view.setZoomFactor(1, 0, false);
		updateSelection();
		updateCursorPosition(false);
	}
	virtual void waveCutListDocumentDidReplaceSourceFile(WaveCutListDocument*, WaveSourceFile::Ptr prevsrcfile, WaveSourceFile::Ptr newsrcfile) override
	{
		view.replaceSourceFile(prevsrcfile, newsrcfile);
	}
	virtual void waveCutListDocumentDidEdit(WaveCutListDocument*, int edittype, const Range64& r) override
	{
		view.setContent(document.getWaveFormat(), document.getWaveCutlist(), false);
		double fs = document.getWaveFormat().sampleRate;
		switch(edittype)
		{
			case WaveCutListDocument::EditType::EditUnknown:
			{
				juce::Range<double> sel = view.getSelectionRange();
				sel.setLength(0);
				view.setSelectionRange(sel);
				break;
			}
			case WaveCutListDocument::EditType::EditInsert:
				view.setSelectionRange(juce::Range<double>((double)r.begin / fs, (double)r.end / fs));
				break;
			case WaveCutListDocument::EditType::EditErase:
				view.setSelectionRange(juce::Range<double>((double)r.begin / fs, (double)r.begin / fs));
				break;
			case WaveCutListDocument::EditType::EditReplace:
				view.setSelectionRange(juce::Range<double>((double)r.begin / fs, (double)r.end / fs));
				break;
		}
		updateSelection();
		updateCursorPosition(false);
	}
	// --------------------------------------------------------------------------------
	// juce::ChangeListener
	virtual void changeListenerCallback(juce::ChangeBroadcaster* source) override
	{
		if(source == &player)
		{
			bool running = player.isRunning();
			if(running && !isTimerRunning()) startTimer(100);
			if(!running && isTimerRunning()) stopTimer();
			if(!running) updateCursorPosition(true);
			applicationCommandManager.commandStatusChanged();
		}
	}
	// --------------------------------------------------------------------------------
	// APIs
	int64_t getCursorPosition64() const
	{
		double fs = document.getWaveFormat().sampleRate;
		double t = view.getCursorPosition();
		return (int64_t)(t * fs);
	}
	Range64 getSelectionRange64() const
	{
		double fs = document.getWaveFormat().sampleRate;
		juce::Range<double> sel = view.getSelectionRange();
		return { (int64_t)(sel.getStart() * fs), (int64_t)(sel.getEnd() * fs) };
	}
};

MainPane::MainPane(juce::ApplicationCommandManager& acm, juce::AudioFormatManager& afm, WaveCutListDocument& doc, WaveCutListPlayer& play) { impl.reset(new Impl(*this, acm, afm, doc, play)); impl->construct(); }
MainPane::~MainPane() { impl.reset(); }
void MainPane::resized() { impl->resized(); }
void MainPane::paint(juce::Graphics& g) { impl->paint(g); }
int64_t MainPane::getCursorPosition64() const { return impl->getCursorPosition64(); }
Range64 MainPane::getSelectionRange64() const { return impl->getSelectionRange64(); }
