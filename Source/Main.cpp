//
//  Main.cpp
//  TestWaveEdit_App
//
//  created by yu2924 on 2023-03-25
//

#include <JuceHeader.h>
#include "WaveCutListDocument.h"
#include "WaveCutListPlayer.h"
#include "MainPane.h"
#include "CommandIDs.h"

// ================================================================================
// SetupWindow

class SetupWindow : public juce::DocumentWindow
{
protected:
	SetupWindow(juce::AudioDeviceManager& adm) : DocumentWindow("Device Setup", juce::LookAndFeel::getDefaultLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId), DocumentWindow::closeButton, true)
	{
		juce::AudioDeviceSelectorComponent* pane = new juce::AudioDeviceSelectorComponent(adm, 0, 0, 2, 2, false, false, true, false);
		pane->setSize(640, 640);
		setUsingNativeTitleBar(true);
		setResizable(true, true);
		setContentOwned(pane, true);
		centreAroundComponent(nullptr, getWidth(), getHeight());
		setVisible(true);
	}
	virtual void closeButtonPressed() override
	{
		closeWindow();
	}
public:
	void closeWindow()
	{
		delete this;
	}
	static juce::Component::SafePointer<SetupWindow> createWindow(juce::AudioDeviceManager& adm)
	{
		return new SetupWindow(adm);
	}
};

// ================================================================================
// toolbar commands

static const char SvgFileOpen[] =
	R"(<svg xmlns="http://www.w3.org/2000/svg">)"
	R"(<path d="m6.6146 2.1167h-3.175l-0.79375-0.79375h-2.1167v5.2917h6.0854l0.79375-3.4396h-5.2917l-0.79375 2.6458" fill="none" stroke="#000" stroke-linecap="round" stroke-linejoin="round" stroke-width=".52917"/>)"
	R"(</svg>)";
static const char SvgFileSave[] =
	R"(<svg xmlns="http://www.w3.org/2000/svg">)"
	R"(<g fill="none" stroke="#000">)"
	R"(<path d="m0.52917 7.1437h6.8792" stroke-width=".52917"/>)"
	R"(<path d="m3.9687 0.52917v5.8565-0.035653" stroke-linecap="round" stroke-width=".52917"/>)"
	R"(<path d="m3.175 5.2917 0.79375 1.094 0.79375-1.094" stroke-linecap="round" stroke-linejoin="round" stroke-width=".52917"/>)"
	R"(</g>)"
	R"(</svg>)";
static const char SvgEditUndo[] =
	R"(<svg xmlns="http://www.w3.org/2000/svg">)"
	R"(<g fill="none" stroke="#000" stroke-linecap="round" stroke-linejoin="round" stroke-width=".52917">)"
	R"(<path d="m1.1153 2.9104a3.175 3.175 0 0 1 4.3371-1.1621 3.175 3.175 0 0 1 1.1621 4.3371" color="#000000"/>)"
	R"(<path d="m0.82479 1.8261 0.29053 1.0843 1.0843-0.29053"/>)"
	R"(</g>)"
	R"(</svg>)";
static const char SvgEditRedo[] =
	R"(<svg xmlns="http://www.w3.org/2000/svg">)"
	R"(<g fill="none" stroke="#000" stroke-linecap="round" stroke-linejoin="round" stroke-width=".52917">)"
	R"(<path d="m6.7487 2.9104a3.175 3.175 0 0 0-4.3371-1.1621 3.175 3.175 0 0 0-1.1621 4.3371" color="#000000"/>)"
	R"(<path d="m7.0392 1.8261-0.29053 1.0843-1.0843-0.29053"/>)"
	R"(</g>)"
	R"(</svg>)";
static const char SvgEditCut[] =
	R"(<svg xmlns="http://www.w3.org/2000/svg">)"
	R"(<g fill="none" stroke="#000" stroke-width=".52917">)"
	R"(<path d="m3.2614 6.5406c1.6435-6.1336 1.6435-6.1336 1.6435-6.1336"/>)"
	R"(<path d="m4.4318 6.5406c-1.6435-6.1336-1.6435-6.1336-1.6435-6.1336"/>)"
	R"(<circle cx="2.4677" cy="6.5406" r=".79375" color="#000000" stroke-linecap="round" stroke-linejoin="round"/>)"
	R"(<circle cx="5.2255" cy="6.5406" r=".79375" color="#000000" stroke-linecap="round" stroke-linejoin="round"/>)"
	R"(</g>)"
	R"(</svg>)";
static const char SvgEditCopy[] =
	R"(<svg xmlns="http://www.w3.org/2000/svg">)"
	R"(<g fill="none" stroke="#000" stroke-linecap="square" stroke-width=".52917">)"
	R"(<path d="m1.5875 6.0854c-0.29316 0-0.52917-0.23601-0.52917-0.52917v-4.4979c0-0.29316 0.23601-0.52917 0.52917-0.52917h3.4396c0.29316 0 0.52917 0.23601 0.52917 0.52917" color="#000000"/>)"
	R"(<rect x="2.3812" y="1.8521" width="4.4979" height="5.5562" rx=".52917" ry=".52917" color="#000000"/>)"
	R"(</g>)"
	R"(</svg>)";
static const char SvgEditPaste[] =
	R"(<svg xmlns="http://www.w3.org/2000/svg">)"
	R"(<g fill="none" stroke="#000" stroke-linecap="square" stroke-width=".52917">)"
	R"(<path d="m2.6458 7.1437h-1.3229c-0.29316 0-0.52917-0.23601-0.52917-0.52917v-5.2917c0-0.29316 0.23601-0.52917 0.52917-0.52917h1.3229" color="#000000"/>)"
	R"(<path d="m5.2917 0.79375h1.3229c0.29316 0 0.52917 0.23601 0.52917 0.52917v1.3229" color="#000000"/>)"
	R"(<rect x="2.6458" y=".26458" width="2.6458" height="1.0583" rx=".52917" ry=".52917" color="#000000"/>)"
	R"(<rect x="2.6458" y="2.6458" width="5.0271" height="5.0271" rx=".52917" ry=".52917" color="#000000"/>)"
	R"(</g>)"
	R"(</svg>)";
static const char SvgEditFadein[] =
	R"(<svg xmlns="http://www.w3.org/2000/svg">)"
	R"(<path d="m7.4083 0.52917-6.6146 6.8792h6.6146" fill="none" stroke="#000" stroke-linecap="round" stroke-linejoin="round" stroke-width=".52917"/>)"
	R"(</svg>)";
static const char SvgEditFadeout[] =
	R"(<svg xmlns="http://www.w3.org/2000/svg">)"
	R"(<path d="m0.52917 0.52917 6.8792 6.8792h-6.8792" fill="none" stroke="#000" stroke-linecap="round" stroke-linejoin="round" stroke-width=".52917"/>)"
	R"(</svg>)";
static const char SvgEditMute[] =
	R"(<svg width="30" height="30" version="1.1" viewBox="0 0 7.9375 7.9375" xmlns="http://www.w3.org/2000/svg">)"
	R"(<path d="m0.52917 0.52917h1.3229v6.8792h4.2333v-6.8792h1.3229" fill="none" stroke="#000" stroke-linecap="round" stroke-linejoin="round" stroke-width=".52917"/>)"
	R"(</svg>)";


class MainToolbarItemFactory : public juce::ToolbarItemFactory
{
public:
	std::unique_ptr<juce::Drawable> loadSvgAsDrawable(const char* svgsz, juce::Colour replacedcolour)
	{
		if(!svgsz) return nullptr;
		std::unique_ptr<juce::Drawable> img = juce::Drawable::createFromImageData(svgsz, juce::CharPointer_ASCII(svgsz).length());
		img->replaceColour(replacedcolour, juce::LookAndFeel::getDefaultLookAndFeel().findColour(juce::Label::ColourIds::textColourId));
		return img;
	}
	juce::ApplicationCommandManager& applicationCommandManager;
	MainToolbarItemFactory(juce::ApplicationCommandManager& acm) : applicationCommandManager(acm)
	{
	}
	virtual void getAllToolbarItemIds(juce::Array<int>& ids) override
	{
		ids =
		{
			juce::ToolbarItemFactory::spacerId,
			juce::ToolbarItemFactory::separatorBarId,
			CommandIDs::FileOpen,
			CommandIDs::FileSave,
			CommandIDs::EditUndo,
			CommandIDs::EditRedo,
			CommandIDs::EditCut,
			CommandIDs::EditCopy,
			CommandIDs::EditPaste,
			CommandIDs::EditFadein,
			CommandIDs::EditFadeout,
			CommandIDs::EditMute,
		};
	}
	virtual void getDefaultItemSet(juce::Array<int> &ids) override
	{
		ids =
		{
			juce::ToolbarItemFactory::spacerId,
			CommandIDs::FileOpen,
			CommandIDs::FileSave,
			juce::ToolbarItemFactory::separatorBarId,
			CommandIDs::EditUndo,
			CommandIDs::EditRedo,
			juce::ToolbarItemFactory::spacerId,
			CommandIDs::EditCut,
			CommandIDs::EditCopy,
			CommandIDs::EditPaste,
			juce::ToolbarItemFactory::spacerId,
			CommandIDs::EditFadein,
			CommandIDs::EditFadeout,
			CommandIDs::EditMute,
		};
	}
	virtual juce::ToolbarItemComponent* createItem(int itemId) override
	{
		static const struct { int cmdid; juce::String label; const char* svgn; const char* svgh; } Table[] =
		{
			{ CommandIDs::FileOpen		, "open"	, SvgFileOpen		, nullptr			},
			{ CommandIDs::FileSave		, "save"	, SvgFileSave		, nullptr			},
			{ CommandIDs::EditUndo		, "undo"	, SvgEditUndo		, nullptr			},
			{ CommandIDs::EditRedo		, "redo"	, SvgEditRedo		, nullptr			},
			{ CommandIDs::EditCut		, "cut"		, SvgEditCut		, nullptr			},
			{ CommandIDs::EditCopy		, "copy"	, SvgEditCopy		, nullptr			},
			{ CommandIDs::EditPaste		, "paste"	, SvgEditPaste		, nullptr			},
			{ CommandIDs::EditFadein	, "fadein"	, SvgEditFadein		, nullptr			},
			{ CommandIDs::EditFadeout	, "fadeout"	, SvgEditFadeout	, nullptr			},
			{ CommandIDs::EditMute		, "mute"	, SvgEditMute		, nullptr			},
		};
		for(int c = juce::numElementsInArray(Table), i = 0; i < c; ++i)
		{
			if(itemId != Table[i].cmdid) continue;
			juce::ToolbarButton* btn = new juce::ToolbarButton(Table[i].cmdid, Table[i].label, loadSvgAsDrawable(Table[i].svgn, juce::Colours::black), loadSvgAsDrawable(Table[i].svgh, juce::Colours::black));
			btn->setCommandToTrigger(&applicationCommandManager, Table[i].cmdid, true);
			return btn;
		}
		return nullptr;
	}
};

// ================================================================================
// MainWindow

class MainWindow : public juce::DocumentWindow, public juce::MenuBarModel, public juce::ApplicationCommandTarget, public juce::FileDragAndDropTarget
{
private:
	juce::ApplicationCommandManager& applicationCommandManager;
	juce::AudioDeviceManager& audioDeviceManager;
	WaveCutListDocument& document;
	WaveCutListPlayer& player;
	class ContentPane : public juce::Component
	{
	public:
		juce::MenuBarComponent menuBarComponent;
		juce::Toolbar toolbar;
		MainPane mainPane;
		enum { ToolBarHeight = 24 };
		ContentPane(juce::ApplicationCommandManager& acm, juce::AudioFormatManager& afm, WaveCutListDocument& doc, WaveCutListPlayer& play) : mainPane(acm, afm, doc, play)
		{
			setOpaque(true);
			addAndMakeVisible(menuBarComponent);
			addAndMakeVisible(toolbar);
			addAndMakeVisible(mainPane);
			setSize(510, 320 - 32);
		}
		virtual void resized() override
		{
			juce::Rectangle<int> rc = getLocalBounds();
			menuBarComponent.setBounds(rc.removeFromTop(getLookAndFeel().getDefaultMenuBarHeight()));
			toolbar.setBounds(rc.removeFromTop(ToolBarHeight));
			mainPane.setBounds(rc);
		}
		virtual void paint(juce::Graphics& g) override
		{
			g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
		}
	}*contentPane;
	juce::Component::SafePointer<SetupWindow> setupWindow;
	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainWindow)
public:
	MainWindow(juce::String name, juce::ApplicationCommandManager& acm, juce::AudioFormatManager& afm, juce::AudioDeviceManager& adm, WaveCutListDocument& doc, WaveCutListPlayer& play)
		: DocumentWindow(name, juce::Desktop::getInstance().getDefaultLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId), DocumentWindow::allButtons)
		, applicationCommandManager(acm)
		, audioDeviceManager(adm)
		, document(doc)
		, player(play)
	{
		setUsingNativeTitleBar(true);
		contentPane = new ContentPane(applicationCommandManager, afm, doc, player);
		setContentOwned(contentPane, true);
		setApplicationCommandManagerToWatch(&applicationCommandManager);
		applicationCommandManager.registerAllCommandsForTarget(this);
		applicationCommandManager.setFirstCommandTarget(this);
		addKeyListener(applicationCommandManager.getKeyMappings());
		contentPane->menuBarComponent.setModel(this);
		contentPane->toolbar.setStyle(juce::Toolbar::iconsOnly);
		MainToolbarItemFactory mtif(applicationCommandManager);
		contentPane->toolbar.addDefaultItems(mtif);
		setResizable(true, true);
		centreWithSize(getWidth(), getHeight());
		setVisible(true);
	}
	virtual ~MainWindow()
	{
		contentPane->menuBarComponent.setModel(nullptr);
		if(setupWindow) setupWindow->closeWindow();
	}
	void closeButtonPressed() override
	{
		juce::JUCEApplication::getInstance()->systemRequestedQuit();
	}
	// --------------------------------------------------------------------------------
	// juce::MenuBarModel
	virtual juce::StringArray getMenuBarNames() override
	{
		return { "File", "Edit", "Transport" };
	}
	virtual juce::PopupMenu getMenuForIndex(int imenu, const juce::String&) override
	{
		juce::PopupMenu menu;
		switch(imenu)
		{
			case 0:
				menu.addCommandItem(&applicationCommandManager, CommandIDs::FileOpen);
				menu.addCommandItem(&applicationCommandManager, CommandIDs::FileSave);
				menu.addCommandItem(&applicationCommandManager, CommandIDs::FileSaveAs);
				menu.addSeparator();
				menu.addCommandItem(&applicationCommandManager, CommandIDs::AppDeviceSetup);
				menu.addSeparator();
				menu.addCommandItem(&applicationCommandManager, CommandIDs::AppExit);
				break;
			case 1:
				menu.addCommandItem(&applicationCommandManager, CommandIDs::EditUndo);
				menu.addCommandItem(&applicationCommandManager, CommandIDs::EditRedo);
				menu.addSeparator();
				menu.addCommandItem(&applicationCommandManager, CommandIDs::EditCut);
				menu.addCommandItem(&applicationCommandManager, CommandIDs::EditCopy);
				menu.addCommandItem(&applicationCommandManager, CommandIDs::EditPaste);
				menu.addCommandItem(&applicationCommandManager, CommandIDs::EditErase);
				menu.addSeparator();
				menu.addCommandItem(&applicationCommandManager, CommandIDs::EditFadein);
				menu.addCommandItem(&applicationCommandManager, CommandIDs::EditFadeout);
				menu.addCommandItem(&applicationCommandManager, CommandIDs::EditMute);
				break;
			case 2:
				menu.addCommandItem(&applicationCommandManager, CommandIDs::TransportRun);
				menu.addCommandItem(&applicationCommandManager, CommandIDs::TransportLoop);
				menu.addCommandItem(&applicationCommandManager, CommandIDs::TransportHome);
				menu.addCommandItem(&applicationCommandManager, CommandIDs::TransportEnd);
				break;
		}
		return menu;
	}
	virtual void menuItemSelected(int, int) override {}
	// --------------------------------------------------------------------------------
	// juce::ApplicationCommandTarget
	virtual juce::ApplicationCommandTarget* getNextCommandTarget() override
	{
		return juce::JUCEApplication::getInstance();
	}
	virtual void getAllCommands(juce::Array<juce::CommandID>& c) override
	{
		juce::Array<juce::CommandID> commands
		{
			CommandIDs::FileOpen,
			CommandIDs::FileSave,
			CommandIDs::FileSaveAs,
			CommandIDs::AppDeviceSetup,
			CommandIDs::AppExit,
			CommandIDs::EditUndo,
			CommandIDs::EditRedo,
			CommandIDs::EditCut,
			CommandIDs::EditCopy,
			CommandIDs::EditPaste,
			CommandIDs::EditErase,
			CommandIDs::EditFadein,
			CommandIDs::EditFadeout,
			CommandIDs::EditMute,
			CommandIDs::TransportRun,
			CommandIDs::TransportLoop,
			CommandIDs::TransportHome,
			CommandIDs::TransportEnd,
		};
		c.addArray(commands);
	}
	virtual void getCommandInfo(juce::CommandID commandID, juce::ApplicationCommandInfo& info) override
	{
		switch(commandID)
		{
			case CommandIDs::FileOpen:
				info.setInfo("Open...", "open RIFF files", "File", 0);
				info.addDefaultKeypress('o', juce::ModifierKeys::commandModifier);
				break;
			case CommandIDs::FileSave:
				info.setInfo("Save", "save", "File", 0);
				info.addDefaultKeypress('s', juce::ModifierKeys::commandModifier);
				info.setActive(document.hasValidContent() && document.hasChangedSinceSaved());
				break;
			case CommandIDs::FileSaveAs:
				info.setInfo("SaveAs", "saveas", "File", 0);
				info.setActive(document.hasValidContent());
				break;
			case CommandIDs::AppDeviceSetup:
				info.setInfo("Setup", "setup", "Device", 0);
				break;
			case CommandIDs::AppExit:
				info.setInfo("Exit", "exit", "Application", 0);
				info.addDefaultKeypress(juce::KeyPress::F4Key, juce::ModifierKeys::altModifier);
				break;
			case CommandIDs::EditUndo:
				info.setInfo("Undo", "undo", "edit", 0);
				info.addDefaultKeypress('z', juce::ModifierKeys::commandModifier);
				info.setActive(document.canUndo());
				break;
			case CommandIDs::EditRedo:
				info.setInfo("Redo", "redo", "edit", 0);
				info.addDefaultKeypress('y', juce::ModifierKeys::commandModifier);
				info.setActive(document.canRedo());
				break;
			case CommandIDs::EditCut:
				info.setInfo("Cut", "cut", "edit", 0);
				info.addDefaultKeypress('x', juce::ModifierKeys::commandModifier);
				info.setActive(document.canCut(contentPane->mainPane.getSelectionRange64()));
				break;
			case CommandIDs::EditCopy:
				info.setInfo("Copy", "copy", "edit", 0);
				info.addDefaultKeypress('c', juce::ModifierKeys::commandModifier);
				info.setActive(document.canCopy(contentPane->mainPane.getSelectionRange64()));
				break;
			case CommandIDs::EditPaste:
				info.setInfo("Paste", "paste", "edit", 0);
				info.addDefaultKeypress('v', juce::ModifierKeys::commandModifier);
				info.setActive(document.canPaste(contentPane->mainPane.getCursorPosition64()));
				break;
			case CommandIDs::EditErase:
				info.setInfo("Erase", "erase", "edit", 0);
				info.addDefaultKeypress(juce::KeyPress::deleteKey, juce::ModifierKeys::noModifiers);
				info.setActive(document.canErase(contentPane->mainPane.getSelectionRange64()));
				break;
			case CommandIDs::EditFadein:
				info.setInfo("Fadein", "fadein", "edit", 0);
				info.setActive(document.canFadein(contentPane->mainPane.getSelectionRange64()));
				break;
			case CommandIDs::EditFadeout:
				info.setInfo("Fadeout", "fadeout", "edit", 0);
				info.setActive(document.canFadeout(contentPane->mainPane.getSelectionRange64()));
				break;
			case CommandIDs::EditMute:
				info.setInfo("Mute", "mute", "edit", 0);
				info.setActive(document.canMute(contentPane->mainPane.getSelectionRange64()));
				break;
			case CommandIDs::TransportRun:
				info.setInfo("Run/Stop", "run/stop", "transport", 0);
				info.addDefaultKeypress(juce::KeyPress::spaceKey, juce::ModifierKeys::noModifiers);
				info.setActive(document.hasValidContent());
				info.setTicked(player.isRunning());
				break;
			case CommandIDs::TransportLoop:
				info.setInfo("Loop", "loop", "transport", 0);
				info.setTicked(player.isLooping());
				break;
			case CommandIDs::TransportHome:
				info.setInfo("Home", "home", "transport", 0);
				info.addDefaultKeypress(juce::KeyPress::homeKey, juce::ModifierKeys::commandModifier);
				info.addDefaultKeypress('w', juce::ModifierKeys::noModifiers);
				info.setActive(document.hasValidContent());
				break;
			case CommandIDs::TransportEnd:
				info.setInfo("End", "end", "transport", 0);
				info.addDefaultKeypress(juce::KeyPress::endKey, juce::ModifierKeys::commandModifier);
				info.setActive(document.hasValidContent());
				break;
		}
	}
	virtual bool perform(const InvocationInfo& info) override
	{
		switch(info.commandID)
		{
			case CommandIDs::FileOpen:
				document.saveIfNeededAndUserAgreesAsync([this](juce::FileBasedDocument::SaveResult r)
				{
					if(r == juce::FileBasedDocument::SaveResult::savedOk) document.loadFromUserSpecifiedFileAsync(true, [this](juce::Result) {});
				});
				return true;
			case CommandIDs::FileSave:
				document.saveAsync(true, true, [this](juce::FileBasedDocument::SaveResult) {});
				return true;
			case CommandIDs::FileSaveAs:
				document.saveAsInteractiveAsync(true, [this](juce::FileBasedDocument::SaveResult) {});
				return true;
			case CommandIDs::AppDeviceSetup:
				if(!setupWindow) setupWindow = SetupWindow::createWindow(audioDeviceManager);
				setupWindow->toFront(true);
				return true;
			case CommandIDs::AppExit:
				juce::JUCEApplication::getInstance()->systemRequestedQuit();
				return true;
			case CommandIDs::EditUndo:
				document.undo();
				return true;
			case CommandIDs::EditRedo:
				document.redo();
				return true;
			case CommandIDs::EditCut:
				document.cut(contentPane->mainPane.getSelectionRange64());
				return true;
			case CommandIDs::EditCopy:
				document.copy(contentPane->mainPane.getSelectionRange64());
				return true;
			case CommandIDs::EditPaste:
				document.paste(contentPane->mainPane.getCursorPosition64());
				return true;
			case CommandIDs::EditErase:
				document.erase(contentPane->mainPane.getSelectionRange64());
				return true;
			case CommandIDs::EditFadein:
				document.fadein(contentPane->mainPane.getSelectionRange64());
				return true;
			case CommandIDs::EditFadeout:
				document.fadeout(contentPane->mainPane.getSelectionRange64());
				return true;
			case CommandIDs::EditMute:
				document.mute(contentPane->mainPane.getSelectionRange64());
				return true;
			case CommandIDs::TransportRun:
				player.setRunning(!player.isRunning());
				return true;
			case CommandIDs::TransportLoop:
				player.setLooping(!player.isLooping());
				return true;
			case CommandIDs::TransportHome:
				player.setPosition(0);
				return true;
			case CommandIDs::TransportEnd:
				player.setPosition(player.getDuration());
				return true;
		}
		return false;
	}
	// --------------------------------------------------------------------------------
	// juce::FileDragAndDropTarget
	virtual bool isInterestedInFileDrag(const juce::StringArray& files) override
	{
		return files.size() == 1;
	}
	virtual void filesDropped(const juce::StringArray& files, int, int) override
	{
		if(files.size() != 1) return;
		juce::File file = files[0];
		document.saveIfNeededAndUserAgreesAsync([this, file](juce::FileBasedDocument::SaveResult r)
		{
			if(r == juce::FileBasedDocument::SaveResult::savedOk) document.loadFrom(juce::File(file), true, true);
		});
	}
};

// ================================================================================
// the Application

class TestWaveEditApplication : public juce::JUCEApplication
{
private:
	juce::ApplicationCommandManager applicationCommandManager;
	juce::AudioFormatManager audioFormatManager;
	juce::AudioDeviceManager audioDeviceManager;
	std::unique_ptr<WaveCutListDocument> document;
	std::unique_ptr<WaveCutListPlayer> player;
	std::unique_ptr<MainWindow> mainWindow;
	juce::TooltipWindow toolTipWindow;
public:
	TestWaveEditApplication() {}
	virtual const juce::String getApplicationName() override { return ProjectInfo::projectName; }
	virtual const juce::String getApplicationVersion() override { return ProjectInfo::versionString; }
	virtual bool moreThanOneInstanceAllowed() override { return true; }
	virtual void initialise(const juce::String&) override
	{
		audioFormatManager.registerBasicFormats();
		audioDeviceManager.initialiseWithDefaultDevices(0, 2);
		document.reset(WaveCutListDocument::createInstance(audioFormatManager));
		player = WaveCutListPlayer::createInstance(audioDeviceManager, *document);
		mainWindow.reset(new MainWindow(getApplicationName(), applicationCommandManager, audioFormatManager, audioDeviceManager, *document, *player));
	}
	virtual void shutdown() override
	{
		mainWindow = nullptr;
		player = nullptr;
		document = nullptr;
		audioDeviceManager.closeAudioDevice();
	}
	virtual void systemRequestedQuit() override
	{
		document->saveIfNeededAndUserAgreesAsync([this](juce::FileBasedDocument::SaveResult r)
		{
			if(r == juce::FileBasedDocument::SaveResult::savedOk) quit();
		});
	}
	virtual void anotherInstanceStarted(const juce::String&) override {}
};

START_JUCE_APPLICATION(TestWaveEditApplication)
