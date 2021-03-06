/*
  For more information, please see: http://software.sci.utah.edu

  The MIT License

  Copyright (c) 2015 Scientific Computing and Imaging Institute,
  University of Utah.

  License for the specific language governing rights and limitations under
  Permission is hereby granted, free of charge, to any person obtaining a
  copy of this software and associated documentation files (the "Software"),
  to deal in the Software without restriction, including without limitation
  the rights to use, copy, modify, merge, publish, distribute, sublicense,
  and/or sell copies of the Software, and to permit persons to whom the
  Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included
  in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
  OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
  THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
  DEALINGS IN THE SOFTWARE.
*/

#include <QtGui>
#include <functional>
#include <boost/bind.hpp>
#include <boost/assign.hpp>
#include <boost/assign/std/vector.hpp>
#include <boost/algorithm/string.hpp>
#include <Core/Utils/Legacy/MemoryUtil.h>
#include <Core/Algorithms/Base/AlgorithmVariableNames.h>
#include <Interface/Application/GuiLogger.h>
#include <Interface/Application/SCIRunMainWindow.h>
#include <Interface/Application/NetworkEditor.h>
#include <Interface/Application/ProvenanceWindow.h>
#include <Interface/Application/DeveloperConsole.h>
#include <Interface/Application/Connection.h>
#include <Interface/Application/PreferencesWindow.h>
#include <Interface/Application/TagManagerWindow.h>
#include <Interface/Application/ShortcutsInterface.h>
#include <Interface/Application/TreeViewCollaborators.h>
#include <Interface/Application/MainWindowCollaborators.h>
#include <Interface/Application/GuiCommands.h>
#include <Interface/Application/NetworkEditorControllerGuiProxy.h>
#include <Interface/Application/NetworkExecutionProgressBar.h>
#include <Interface/Application/DialogErrorControl.h>
#include <Interface/Modules/Base/RemembersFileDialogDirectory.h>
#include <Interface/Modules/Base/ModuleDialogGeneric.h> //TODO
#include <Interface/Application/ModuleWizard/ModuleWizard.h>
#include <Dataflow/Network/NetworkFwd.h>
#include <Dataflow/Engine/Controller/NetworkEditorController.h> //DOH! see TODO in setController
#include <Dataflow/Engine/Controller/ProvenanceManager.h>
#include <Dataflow/Network/SimpleSourceSink.h>  //TODO: encapsulate!!!
#include <Dataflow/Serialization/Network/XMLSerializer.h>
#include <Core/Application/Application.h>
#include <Core/Application/Preferences/Preferences.h>
#include <Core/Logging/Log.h>
#include <Core/Application/Version.h>
#include <Dataflow/Serialization/Network/NetworkDescriptionSerialization.h>
#include <Core/Utils/CurrentFileName.h>

#ifdef BUILD_WITH_PYTHON
#include <Interface/Application/PythonConsoleWidget.h>
#include <Core/Python/PythonInterpreter.h>
#endif
#include "TriggeredEventsWindow.h"
#include <Dataflow/Serialization/Network/XMLSerializer.h>

using namespace SCIRun;
using namespace SCIRun::Gui;
using namespace SCIRun::Dataflow::Engine;
using namespace SCIRun::Dataflow::Networks;
using namespace SCIRun::Dataflow::State;
using namespace SCIRun::Core::Commands;
using namespace SCIRun::Core::Logging;
using namespace SCIRun::Core;
using namespace SCIRun::Core::Algorithms;

SCIRunMainWindow::SCIRunMainWindow()
{
  setupUi(this);
  builder_ = boost::make_shared<NetworkEditorBuilder>(this);
  dockManager_ = new DockManager(dockSpace_, this);

  startup_ = true;

  QCoreApplication::setOrganizationName("SCI:CIBC Software");
  QCoreApplication::setApplicationName("SCIRun5");

  setAttribute(Qt::WA_DeleteOnClose);

  if (newInterface())
    setStyleSheet(
    "background-color: rgb(66,66,69);"
    "color: white;"
    "selection-color: yellow;"
    "selection-background-color: blue;"//336699 lighter blue
    "QToolBar {        background-color: rgb(66,66,69); border: 1px solid black; color: black;     }"
    "QProgressBar {        background-color: rgb(66,66,69); border: 0px solid black; color: black  ;   }"
    "QDockWidget {background: rgb(66,66,69); background-color: rgb(66,66,69); }"
    "QPushButton {"
    "  border: 2px solid #8f8f91;"
    "  border - radius: 6px;"
    "  background - color: qlineargradient(x1 : 0, y1 : 0, x2 : 0, y2 : 1,"
    "  stop : 0 #f6f7fa, stop: 1 #dadbde);"
    "  min - width: 80px;"
    "}"
    "QPushButton:pressed{"
    "background - color: qlineargradient(x1 : 0, y1 : 0, x2 : 0, y2 : 1,"
    "stop : 0 #dadbde, stop: 1 #f6f7fa);"
    "}"
    "QPushButton:flat{"
    "          border: none; /* no border for a flat push button */"
    "}"
    "QPushButton:default {"
    "border - color: navy; /* make the default button prominent */"
    "}"
    );

  menubar_->setStyleSheet("QMenuBar::item::selected{background-color : rgb(66, 66, 69); } QMenuBar::item::!selected{ background-color : rgb(66, 66, 69); } ");

  dialogErrorControl_.reset(new DialogErrorControl(this));
  setupTagManagerWindow();

  setupPreferencesWindow();

  setupNetworkEditor();

  setTipsAndWhatsThis();

  connect(actionExecuteAll_, SIGNAL(triggered()), this, SLOT(executeAll()));
  connect(actionNew_, SIGNAL(triggered()), this, SLOT(newNetwork()));

  setActionIcons();

  createStandardToolbars();
  createExecuteToolbar();
  createAdvancedToolbar();

  #ifdef __APPLE__
  connect(actionLaunchNewInstance_, SIGNAL(triggered()), this, SLOT(launchNewInstance()));
  #else
  menuFile_->removeAction(actionLaunchNewInstance_);
  #endif

  {
    auto searchAction = new QWidgetAction(this);
    searchAction->setDefaultWidget(new NetworkSearchWidget(networkEditor_));
    addToolBarBreak();
    auto searchBar = addToolBar("&Search");
    searchBar->setObjectName("SearchToolBar");
    WidgetStyleMixin::toolbarStyle(searchBar);
    searchBar->addAction(searchAction);
    connect(actionSearchBar_, SIGNAL(toggled(bool)), searchBar, SLOT(setVisible(bool)));
    connect(searchBar, SIGNAL(visibilityChanged(bool)), actionSearchBar_, SLOT(setChecked(bool)));
    searchBar->setVisible(false);
  }

  setContextMenuPolicy(Qt::NoContextMenu);

  scrollArea_->viewport()->setBackgroundRole(QPalette::Dark);
  scrollArea_->viewport()->setAutoFillBackground(true);
  scrollArea_->setStyleSheet(styleSheet());

  connect(actionSave_As_, SIGNAL(triggered()), this, SLOT(saveNetworkAs()));
  connect(actionSave_, SIGNAL(triggered()), this, SLOT(saveNetwork()));
  connect(actionLoad_, SIGNAL(triggered()), this, SLOT(loadNetwork()));
  connect(actionImportNetwork_, SIGNAL(triggered()), this, SLOT(importLegacyNetwork()));
  connect(actionQuit_, SIGNAL(triggered()), this, SLOT(close()));
  connect(actionRunScript_, SIGNAL(triggered()), this, SLOT(runScript()));

  actionQuit_->setShortcut(QKeySequence::Quit);

  actionDelete_->setShortcuts(QList<QKeySequence>() << QKeySequence::Delete << Qt::Key_Backspace);

	connect(actionRunNewModuleWizard_, SIGNAL(triggered()), this, SLOT(runNewModuleWizard()));
	actionRunNewModuleWizard_->setDisabled(true);

  connect(actionAbout_, SIGNAL(triggered()), this, SLOT(displayAcknowledgement()));
  connect(actionCreateToolkitFromDirectory_, SIGNAL(triggered()), this, SLOT(helpWithToolkitCreation()));

  connect(helpActionPythonAPI_, SIGNAL(triggered()), this, SLOT(loadPythonAPIDoc()));
  connect(helpActionSnippets_, SIGNAL(triggered()), this, SLOT(showSnippetHelp()));
  connect(helpActionClipboard_, SIGNAL(triggered()), this, SLOT(showClipboardHelp()));
	connect(helpActionTagLayer_, SIGNAL(triggered()), this, SLOT(showTagHelp()));
	connect(helpActionTriggeredScripts_, SIGNAL(triggered()), this, SLOT(showTriggerHelp()));
  connect(helpActionNewUserWizard_, SIGNAL(triggered()), this, SLOT(launchNewUserWizard()));

  connect(actionReset_Window_Layout, SIGNAL(triggered()), this, SLOT(resetWindowLayout()));

#ifndef BUILD_WITH_PYTHON
  actionRunScript_->setEnabled(false);
#endif

  for (int i = 0; i < MaxRecentFiles; ++i)
  {
    recentFileActions_.push_back(new QAction(this));
    recentFileActions_[i]->setVisible(false);
    recentNetworksMenu_->addAction(recentFileActions_[i]);
    connect(recentFileActions_[i], SIGNAL(triggered()), this, SLOT(loadRecentNetwork()));
  }

	setupScriptedEventsWindow();
  setupProvenanceWindow();

  setupDevConsole();
  setupPythonConsole();

  connect(prefsWindow_->defaultNotePositionComboBox_, SIGNAL(activated(int)), this, SLOT(readDefaultNotePosition(int)));
  connect(prefsWindow_->defaultNoteSizeComboBox_, SIGNAL(activated(int)), this, SLOT(readDefaultNoteSize(int)));
  connect(prefsWindow_->cubicPipesRadioButton_, SIGNAL(clicked()), this, SLOT(makePipesCubicBezier()));
  connect(prefsWindow_->manhattanPipesRadioButton_, SIGNAL(clicked()), this, SLOT(makePipesManhattan()));
  connect(prefsWindow_->euclideanPipesRadioButton_, SIGNAL(clicked()), this, SLOT(makePipesEuclidean()));
  //TODO: will be a user or network setting
  makePipesEuclidean();

  connect(moduleFilterLineEdit_, SIGNAL(textChanged(const QString&)), this, SLOT(filterModuleNamesInTreeView(const QString&)));

  connect(prefsWindow_->modulesSnapToCheckBox_, SIGNAL(stateChanged(int)), this, SLOT(modulesSnapToChanged()));
  connect(prefsWindow_->modulesSnapToCheckBox_, SIGNAL(stateChanged(int)), networkEditor_, SIGNAL(snapToModules()));
  connect(prefsWindow_->portSizeEffectsCheckBox_, SIGNAL(stateChanged(int)), this, SLOT(highlightPortsChanged()));
  connect(prefsWindow_->portSizeEffectsCheckBox_, SIGNAL(stateChanged(int)), networkEditor_, SIGNAL(highlightPorts(int)));
  connect(prefsWindow_->dockableModulesCheckBox_, SIGNAL(stateChanged(int)), this, SLOT(adjustModuleDock(int)));

  makeFilterButtonMenu();

  connect(prefsWindow_->scirunDataPushButton_, SIGNAL(clicked()), this, SLOT(setDataDirectoryFromGUI()));
  connect(prefsWindow_->addToPathButton_, SIGNAL(clicked()), this, SLOT(addToPathFromGUI()));
  connect(actionFilter_modules_, SIGNAL(triggered()), this, SLOT(setFocusOnFilterLine()));
  connect(actionAddModule_, SIGNAL(triggered()), this, SLOT(addModuleKeyboardAction()));
  actionAddModule_->setVisible(false);
  connect(actionSelectModule_, SIGNAL(triggered()), this, SLOT(selectModuleKeyboardAction()));
  actionSelectModule_->setVisible(false);
  connect(actionReportIssue_, SIGNAL(triggered()), this, SLOT(reportIssue()));
  connect(actionSelectMode_, SIGNAL(toggled(bool)), this, SLOT(setSelectMode(bool)));
  connect(actionDragMode_, SIGNAL(toggled(bool)), this, SLOT(setDragMode(bool)));
	connect(actionToggleTagLayer_, SIGNAL(toggled(bool)), this, SLOT(toggleTagLayer(bool)));
  connect(actionToggleMetadataLayer_, SIGNAL(toggled(bool)), this, SLOT(toggleMetadataLayer(bool)));
  connect(actionResetNetworkZoom_, SIGNAL(triggered()), this, SLOT(zoomNetwork()));
  connect(actionZoomIn_, SIGNAL(triggered()), this, SLOT(zoomNetwork()));
  connect(actionZoomOut_, SIGNAL(triggered()), this, SLOT(zoomNetwork()));
  connect(actionZoomBestFit_, SIGNAL(triggered()), this, SLOT(zoomNetwork()));

  actionCut_->setIcon(QPixmap(":/general/Resources/cut.png"));
  actionCopy_->setIcon(QPixmap(":/general/Resources/copy.png"));
  actionPaste_->setIcon(QPixmap(":/general/Resources/paste.png"));

  connect(actionKeyboardShortcuts_, SIGNAL(triggered()), this, SLOT(showKeyboardShortcutsDialog()));

  //TODO: store in xml file, add to app resources
  {
    ToolkitInfo fwdInv{ "http://www.sci.utah.edu/images/software/forward-inverse/forward-inverse-mod.png",
    #ifdef __APPLE__
      "https://codeload.github.com/SCIInstitute/FwdInvToolkit/zip/v1.4",
    #else
      "http://sci.utah.edu/devbuilds/scirun5/toolkits/FwdInvToolkit_v1.4.zip",
    #endif
      "FwdInvToolkit_stable.zip" };
    fwdInv.setupAction(actionForwardInverseStable_, this);
  }
  {
    ToolkitInfo fwdInvNightly{ "http://www.sci.utah.edu/images/software/forward-inverse/forward-inverse-mod.png",
      "https://codeload.github.com/SCIInstitute/FwdInvToolkit/zip/master",
      "FwdInvToolkit_nightly.zip" };
    fwdInvNightly.setupAction(actionForwardInverseNightly_, this);
  }
  {
    ToolkitInfo brainStim{ "http://www.sci.utah.edu/images/software/BrainStimulator/brain-stimulator-mod.png",
    #ifdef __APPLE__
      "https://codeload.github.com/SCIInstitute/BrainStimulator/zip/BrainStimulator_v1.3",
    #else
      "http://sci.utah.edu/devbuilds/scirun5/toolkits/BrainStimulator_v1.3.zip",
    #endif
      "BrainStimulator_stable.zip" };
    brainStim.setupAction(actionBrainStimulatorStable_, this);
  }
  {
    ToolkitInfo brainStimNightly{ "http://www.sci.utah.edu/images/software/BrainStimulator/brain-stimulator-mod.png",
      "https://codeload.github.com/SCIInstitute/BrainStimulator/zip/master",
      "BrainStimulator_nightly.zip" };
    brainStimNightly.setupAction(actionBrainStimulatorNightly_, this);
  }
  connect(actionLoadToolkit_, SIGNAL(triggered()), this, SLOT(loadToolkit()));

  connect(networkEditor_, SIGNAL(networkExecuted()), networkProgressBar_.get(), SLOT(resetModulesDone()));
  connect(networkEditor_->moduleEventProxy().get(), SIGNAL(moduleExecuteEnd(double, const std::string&)), networkProgressBar_.get(), SLOT(incrementModulesDone(double, const std::string&)));
  connect(networkEditor_, SIGNAL(networkExecuted()), dialogErrorControl_.get(), SLOT(resetCounter()));
	connect(networkEditor_, SIGNAL(requestLoadNetwork(const QString&)), this, SLOT(checkAndLoadNetworkFile(const QString&)));
  connect(networkEditor_, SIGNAL(networkExecuted()), this, SLOT(changeExecuteActionIconToStop()));

  connect(prefsWindow_->actionTextIconCheckBox_, SIGNAL(clicked()), this, SLOT(adjustExecuteButtonAppearance()));
  prefsWindow_->actionTextIconCheckBox_->setCheckState(Qt::PartiallyChecked);
  adjustExecuteButtonAppearance();

  connect(networkEditor_, SIGNAL(newSubnetworkCopied(const QString&)), this, SLOT(updateClipboardHistory(const QString&)));
  connect(networkEditor_, SIGNAL(middleMouseClicked()), this, SLOT(switchMouseMode()));

  connect(openLogFolderButton_, SIGNAL(clicked()), this, SLOT(openLogFolder()));

  setupInputWidgets();

  logTextBrowser_->append("Hello! Welcome to SCIRun 5.");
  readSettings();

  setCurrentFile("");

  actionConfiguration_->setChecked(!configurationDockWidget_->isHidden());
  actionModule_Selector->setChecked(!moduleSelectorDockWidget_->isHidden());
  actionProvenance_->setChecked(!provenanceWindow_->isHidden());
  actionTriggeredEvents_->setChecked(!triggeredEventsWindow_->isHidden());
  actionTagManager_->setChecked(!tagManagerWindow_->isHidden());

	moduleSelectorDockWidget_->setStyleSheet("QDockWidget {background: rgb(66,66,69); background-color: rgb(66,66,69) }"
		"QToolTip { color: #ffffff; background - color: #2a82da; border: 1px solid white; }"
		"QHeaderView::section { background: rgb(66,66,69);} "
		);

  hideNonfunctioningWidgets();

  connect(moduleSelectorDockWidget_, SIGNAL(topLevelChanged(bool)), this, SLOT(updateDockWidgetProperties(bool)));

  setupVersionButton();

  WidgetStyleMixin::tabStyle(optionsTabWidget_);

  //devConsole_->updateNetworkViewLog("hello");
}

void SCIRunMainWindow::resizeEvent(QResizeEvent* event)
{
  dockSpace_ = size().height();
  QMainWindow::resizeEvent(event);

  //devConsole_->updateNetworkViewLog(tr("resizeEvent to %1,%2").arg(size().width()).arg(size().height()));
}

void SCIRunMainWindow::createStandardToolbars()
{
  auto standardBar = addToolBar("File");
  WidgetStyleMixin::toolbarStyle(standardBar);
  standardBar->setObjectName("FileToolBar");
  standardBar->addAction(actionNew_);
  standardBar->addAction(actionLoad_);
  standardBar->addAction(actionSave_);
  standardBar->addAction(actionEnterWhatsThisMode_);
  standardBar->addAction(actionDragMode_);
  standardBar->addAction(actionSelectMode_);

  auto networkBar = addToolBar("Network");
  addNetworkActionsToBar(networkBar);

  actionZoomBestFit_->setDisabled(true);

  connect(actionFileBar_, SIGNAL(toggled(bool)), standardBar, SLOT(setVisible(bool)));
  connect(standardBar, SIGNAL(visibilityChanged(bool)), actionFileBar_, SLOT(setChecked(bool)));

  connect(actionNetworkBar_, SIGNAL(toggled(bool)), networkBar, SLOT(setVisible(bool)));
  connect(networkBar, SIGNAL(visibilityChanged(bool)), actionNetworkBar_, SLOT(setChecked(bool)));
}

void SCIRunMainWindow::addNetworkActionsToBar(QToolBar* toolbar) const
{
  WidgetStyleMixin::toolbarStyle(toolbar);
  toolbar->setObjectName("NetworkToolBar");

  toolbar->addAction(actionPinAllModuleUIs_);
  toolbar->addAction(actionRestoreAllModuleUIs_);
  toolbar->addAction(actionHideAllModuleUIs_);
  toolbar->addSeparator();
  toolbar->addAction(actionCenterNetworkViewer_);
  toolbar->addAction(actionZoomIn_);
  toolbar->addAction(actionZoomOut_);
  toolbar->addAction(actionResetNetworkZoom_);
}

void SCIRunMainWindow::createAdvancedToolbar()
{
  auto advancedBar = addToolBar("Advanced");
  WidgetStyleMixin::toolbarStyle(advancedBar);
  advancedBar->setObjectName("AdvancedToolBar");

  advancedBar->addAction(actionRunScript_);
  advancedBar->addAction(actionToggleMetadataLayer_);
  advancedBar->addAction(actionToggleTagLayer_);
  //TODO: turn back on after IBBM
  //advancedBar->addAction(actionMakeSubnetwork_);
  advancedBar->addActions(networkProgressBar_->advancedActions());

  connect(actionAdvancedBar_, SIGNAL(toggled(bool)), advancedBar, SLOT(setVisible(bool)));
  connect(advancedBar, SIGNAL(visibilityChanged(bool)), actionAdvancedBar_, SLOT(setChecked(bool)));

  advancedBar->setVisible(false);
}

void SCIRunMainWindow::createExecuteToolbar()
{
  auto executeBar = addToolBar(tr("&Execute"));
  executeBar->setObjectName("ExecuteToolBar");

  executeButton_ = new QToolButton;
  executeButton_->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  executeButton_->addAction(actionExecuteAll_);
  executeButton_->setDefaultAction(actionExecuteAll_);
  executeBar->addWidget(executeButton_);

  networkProgressBar_.reset(new NetworkExecutionProgressBar(boost::make_shared<NetworkStatusImpl>(networkEditor_), this));
  executeBar->addActions(networkProgressBar_->mainActions());
  executeBar->setStyleSheet("QToolBar { background-color: rgb(66,66,69); border: 1px solid black; color: black }"
    "QToolTip { color: #ffffff; background - color: #2a82da; border: 1px solid white; }"
    );
  executeBar->setAutoFillBackground(true);
  connect(actionExecuteBar_, SIGNAL(toggled(bool)), executeBar, SLOT(setVisible(bool)));
  connect(executeBar, SIGNAL(visibilityChanged(bool)), actionExecuteBar_, SLOT(setChecked(bool)));
}

void SCIRunMainWindow::initialize()
{
  postConstructionSignalHookup();

  fillModuleSelector();

  executeCommandLineRequests();
}

void SCIRunMainWindow::postConstructionSignalHookup()
{
  connect(moduleSelectorTreeWidget_, SIGNAL(itemDoubleClicked(QTreeWidgetItem*, int)), this, SLOT(filterDoubleClickedModuleSelectorItem(QTreeWidgetItem*)));
	moduleSelectorTreeWidget_->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(moduleSelectorTreeWidget_, SIGNAL(customContextMenuRequested(const QPoint&)), this, SLOT(showModuleSelectorContextMenu(const QPoint&)));

  WidgetDisablingService::Instance().addNetworkEditor(networkEditor_);
  connect(networkEditor_->getNetworkEditorController().get(), SIGNAL(executionStarted()), &WidgetDisablingService::Instance(), SLOT(disableInputWidgets()));
  connect(networkEditor_->getNetworkEditorController().get(), SIGNAL(executionFinished(int)), &WidgetDisablingService::Instance(), SLOT(enableInputWidgets()));
  connect(networkEditor_->getNetworkEditorController().get(), SIGNAL(executionFinished(int)), this, SLOT(changeExecuteActionIconToPlay()));
  connect(networkEditor_->getNetworkEditorController().get(), SIGNAL(executionFinished(int)), this, SLOT(alertForNetworkCycles(int)));

	connect(networkEditor_, SIGNAL(disableWidgetDisabling()), &WidgetDisablingService::Instance(), SLOT(temporarilyDisableService()));
  connect(networkEditor_, SIGNAL(reenableWidgetDisabling()), &WidgetDisablingService::Instance(), SLOT(temporarilyEnableService()));

  connect(networkEditor_->getNetworkEditorController().get(), SIGNAL(moduleRemoved(const SCIRun::Dataflow::Networks::ModuleId&)),
    networkEditor_, SLOT(removeModuleWidget(const SCIRun::Dataflow::Networks::ModuleId&)));

  connect(networkEditor_->getNetworkEditorController().get(), SIGNAL(moduleAdded(const std::string&, SCIRun::Dataflow::Networks::ModuleHandle, const SCIRun::Dataflow::Engine::ModuleCounter&)),
    commandConverter_.get(), SLOT(moduleAdded(const std::string&, SCIRun::Dataflow::Networks::ModuleHandle)));
  connect(networkEditor_->getNetworkEditorController().get(), SIGNAL(moduleRemoved(const SCIRun::Dataflow::Networks::ModuleId&)),
    commandConverter_.get(), SLOT(moduleRemoved(const SCIRun::Dataflow::Networks::ModuleId&)));
  connect(networkEditor_->getNetworkEditorController().get(), SIGNAL(connectionAdded(const SCIRun::Dataflow::Networks::ConnectionDescription&)),
    commandConverter_.get(), SLOT(connectionAdded(const SCIRun::Dataflow::Networks::ConnectionDescription&)));
  connect(networkEditor_->getNetworkEditorController().get(), SIGNAL(connectionRemoved(const SCIRun::Dataflow::Networks::ConnectionId&)),
    commandConverter_.get(), SLOT(connectionRemoved(const SCIRun::Dataflow::Networks::ConnectionId&)));
  connect(networkEditor_, SIGNAL(moduleMoved(const SCIRun::Dataflow::Networks::ModuleId&, double, double)),
    commandConverter_.get(), SLOT(moduleMoved(const SCIRun::Dataflow::Networks::ModuleId&, double, double)));
  connect(provenanceWindow_, SIGNAL(modifyingNetwork(bool)), commandConverter_.get(), SLOT(networkBeingModifiedByProvenanceManager(bool)));
  connect(networkEditor_, SIGNAL(newModule(const QString&, bool)), this, SLOT(addModuleToWindowList(const QString&, bool)));
  connect(networkEditor_->getNetworkEditorController().get(), SIGNAL(moduleRemoved(const SCIRun::Dataflow::Networks::ModuleId&)),
    this, SLOT(removeModuleFromWindowList(const SCIRun::Dataflow::Networks::ModuleId&)));

  for (const auto& t : toolkitFiles_)
  {
    loadToolkitsFromFile(t);
  }

  startup_ = false;
}

void SCIRunMainWindow::setTipsAndWhatsThis()
{
  actionExecuteAll_->setStatusTip(tr("Execute all modules"));
  actionExecuteAll_->setWhatsThis(tr("Click this option to execute all modules in the current network editor."));
  actionNew_->setStatusTip(tr("New network"));
  actionNew_->setWhatsThis(tr("Click this option to start editing a blank network file."));
  actionSave_->setStatusTip(tr("Save network"));
  actionSave_->setWhatsThis(tr("Click this option to save the current network to disk."));
  actionLoad_->setStatusTip(tr("Load network"));
  actionLoad_->setWhatsThis(tr("Click this option to load a new network file from disk."));
  actionEnterWhatsThisMode_ = QWhatsThis::createAction(this);
  actionEnterWhatsThisMode_->setStatusTip(tr("Enter What's This? Mode"));
  actionEnterWhatsThisMode_->setShortcuts(QList<QKeySequence>() << tr("Ctrl+H") << tr("F1"));
  actionHideAllModuleUIs_->setWhatsThis("Hides all module UI windows.");
  actionRestoreAllModuleUIs_->setWhatsThis("Restores all module UI windows.");
  actionPinAllModuleUIs_->setWhatsThis("Pins all module UI windows to right side of main window.");
  //todo: zoom actions, etc
}

void SCIRunMainWindow::setupInputWidgets()
{
  // will be slicker in C++11
  using namespace boost::assign;
  std::vector<InputWidget> widgets;
  widgets += actionExecuteAll_,
    actionSave_,
    actionLoad_,
    actionSave_As_,
    actionNew_,
    actionDelete_,
    moduleSelectorTreeWidget_,
    actionRunScript_;

  WidgetDisablingService::Instance().addWidgets(widgets.begin(), widgets.end());
  WidgetDisablingService::Instance().addWidgets(recentFileActions_.begin(), recentFileActions_.end());
}

SCIRunMainWindow* SCIRunMainWindow::instance_ = nullptr;

SCIRunMainWindow* SCIRunMainWindow::Instance()
{
  if (!instance_)
  {
    instance_ = new SCIRunMainWindow;
  }
  return instance_;
}

SCIRunMainWindow::~SCIRunMainWindow()
{
  GuiLogger::setInstance(nullptr);
  Log::get().clearAppenders();
  Log::get("Modules").clearAppenders();
  commandConverter_.reset();
  networkEditor_->disconnect();
  networkEditor_->setNetworkEditorController(nullptr);
  networkEditor_->clear();
  Application::Instance().shutdown();
}

void SCIRunMainWindow::setController(NetworkEditorControllerHandle controller)
{
  auto controllerProxy(boost::make_shared<NetworkEditorControllerGuiProxy>(controller, networkEditor_));
  networkEditor_->setNetworkEditorController(controllerProxy);
  //TODO: need better way to wire this up
  controller->setSerializationManager(networkEditor_);
}

void SCIRunMainWindow::setupNetworkEditor()
{
  boost::shared_ptr<TreeViewModuleGetter> getter(new TreeViewModuleGetter(*moduleSelectorTreeWidget_));
	const bool regression = Application::Instance().parameters()->isRegressionMode();
  boost::shared_ptr<TextEditAppender> logger(new TextEditAppender(logTextBrowser_, regression));
  GuiLogger::setInstance(logger);
  Log::get().addCustomAppender(logger);
  //TODO: this logger will crash on Windows when the console is closed. See #1250. Need to figure out a better way to manage scope/lifetime of Qt widgets passed to global singletons...
  //boost::shared_ptr<TextEditAppender> moduleLog(new TextEditAppender(moduleLogTextBrowser_));
  //Log::get("Modules").addCustomAppender(moduleLog);
  defaultNotePositionGetter_.reset(new ComboBoxDefaultNotePositionGetter(prefsWindow_->defaultNotePositionComboBox_, prefsWindow_->defaultNoteSizeComboBox_));
  auto tagColorFunc = [this](int tag) { return tagManagerWindow_->tagColor(tag); };
  auto tagNameFunc = [this](int tag) { return tagManagerWindow_->tagName(tag); };
	auto preexecuteFunc = [this]() { preexecute(); };
  auto highResolutionExpandFactor = Core::Application::Instance().parameters()->developerParameters()->guiExpandFactor().get_value_or(1.0);
  {
    auto screen = QApplication::desktop()->screenGeometry();
    if (screen.height() * screen.width() > 4096000) // 2560x1600
      highResolutionExpandFactor = NetworkBoundaries::highDPIExpandFactorDefault;
  }
  networkEditor_ = new NetworkEditor({ getter, defaultNotePositionGetter_, dialogErrorControl_, preexecuteFunc,
    tagColorFunc, tagNameFunc, highResolutionExpandFactor, dockManager_ }, scrollAreaWidgetContents_);
  gridLayout_5->addWidget(networkEditor_, 0, 0, 1, 1);

  builder_->connectAll(networkEditor_);
  NetworkEditor::setConnectorFunc([this](NetworkEditor* ed) { builder_->connectAll(ed); });
}

void SCIRunMainWindow::executeCommandLineRequests()
{
  Application::Instance().executeCommandLineRequests();
}

void SCIRunMainWindow::preexecute()
{
	if (Preferences::Instance().saveBeforeExecute && !Application::Instance().parameters()->isRegressionMode())
	{
		saveNetwork();
	}
}

void SCIRunMainWindow::executeAll()
{
	if (Application::Instance().parameters()->isRegressionMode())
	{
		auto timeout = Application::Instance().parameters()->developerParameters()->regressionTimeoutSeconds();
		QTimer::singleShot(1000 * *timeout, this, SLOT(networkTimedOut()));
	}
  writeSettings();
  networkEditor_->executeAll();
}

void SCIRunMainWindow::setupQuitAfterExecute()
{
  connect(networkEditor_->getNetworkEditorController().get(), SIGNAL(executionFinished(int)), this, SLOT(exitApplication(int)));
  quitAfterExecute_ = true;
}

void SCIRunMainWindow::exitApplication(int code)
{
  close();
  returnCode_ = code;
  qApp->exit(code);
}

void SCIRunMainWindow::quit()
{
  exitApplication(0);
}

void SCIRunMainWindow::networkTimedOut()
{
	exitApplication(2);
}

void SCIRunMainWindow::saveNetwork()
{
  if (currentFile_.isEmpty())
    saveNetworkAs();
  else
    saveNetworkFile(currentFile_);
}

void SCIRunMainWindow::saveNetworkAs()
{
  auto filename = QFileDialog::getSaveFileName(this, "Save Network...", latestNetworkDirectory_.path(), "*.srn5");
  if (!filename.isEmpty())
    saveNetworkFile(filename);
}

void SCIRunMainWindow::saveNetworkFile(const QString& fileName)
{
  writeSettings();
  NetworkSaveCommand save;
  save.set(Variables::Filename, fileName.toStdString());
  save.execute();
}

void SCIRunMainWindow::loadNetwork()
{
  if (okToContinue())
  {
    auto filename = QFileDialog::getOpenFileName(this, "Load Network...", latestNetworkDirectory_.path(), "*.srn5");
    loadNetworkFile(filename);
  }
}

void SCIRunMainWindow::checkAndLoadNetworkFile(const QString& filename)
{
  if (okToContinue())
  {
    loadNetworkFile(filename);
  }
}

bool SCIRunMainWindow::loadNetworkFile(const QString& filename, bool isTemporary)
{
  if (!filename.isEmpty())
  {
    FileOpenCommand command;
    command.set(Variables::Filename, filename.toStdString());
    command.set(Name("temporary-file"), isTemporary);
    if (command.execute())
    {
      networkProgressBar_->updateTotalModules(networkEditor_->numModules());
      if (!isTemporary)
      {
        setCurrentFile(filename);
        statusBar()->showMessage(tr("File loaded: ") + filename, 2000);
        provenanceWindow_->clear();
        provenanceWindow_->showFile(command.file_);
      }
      else
      {
        setCurrentFile("");
        setWindowModified(true);
        showStatusMessage("Toolkit network loaded. ", 2000);
      }
			networkEditor_->viewport()->update();
      return true;
    }
    else
    {
      if (Application::Instance().parameters()->isRegressionMode())
        exit(7);
      //TODO: set error code to non-0 so regression tests fail!
      // probably want to control this with a --regression flag.
    }
  }
  return false;
}

void SCIRunMainWindow::importLegacyNetwork()
{
  if (okToContinue())
  {
    auto filename = QFileDialog::getOpenFileName(this, "Import Old Network...", latestNetworkDirectory_.path(), "*.srn");
    importLegacyNetworkFile(filename);
  }
}

bool SCIRunMainWindow::importLegacyNetworkFile(const QString& filename)
{
	bool success = false;
  if (!filename.isEmpty())
  {
    FileImportCommand command;
    command.set(Variables::Filename, filename.toStdString());
    if (command.execute())
    {
      statusBar()->showMessage(tr("File imported: ") + filename, 2000);
      networkProgressBar_->updateTotalModules(networkEditor_->numModules());
      networkEditor_->viewport()->update();
      success = true;
    }
    else
    {
      statusBar()->showMessage(tr("File import failed: ") + filename, 2000);
    }
		auto log = QString::fromStdString(command.logContents());
		auto logFileName = latestNetworkDirectory_.path() + "/" + ("importLog_" + strippedName(filename) + ".log");
		QFile logFile(logFileName); //todo: add timestamp
    if (logFile.open(QFile::WriteOnly | QFile::Text))
		{
			QTextStream stream(&logFile);
			stream << log;
			QMessageBox::information(this, "SRN File Import", "SRN File Import log file can be found here: " + logFileName
				+ "\n\nAdditionally, check the log directory for a list of missing modules (look for file missingModules.log)");
    }
		else
		{
			QMessageBox::information(this, "SRN File Import", "Failed to write SRN File Import log file: " + logFileName);
		}
  }
  return success;
}

bool SCIRunMainWindow::newNetwork()
{
  if (okToContinue())
  {
    networkEditor_->clear();
    provenanceWindow_->clear();
    setCurrentFile("");
		networkEditor_->viewport()->update();
    return true;
  }
  return false;
}

void SCIRunMainWindow::setCurrentFile(const QString& fileName)
{
  currentFile_ = fileName;
  setCurrentFileName(currentFile_.toStdString());
  setWindowModified(false);
  auto shownName = tr("Untitled");
  if (!currentFile_.isEmpty())
  {
    shownName = strippedName(currentFile_);
    latestNetworkDirectory_ = QFileInfo(currentFile_).dir();
    recentFiles_.removeAll(currentFile_);
    recentFiles_.prepend(currentFile_);
    updateRecentFileActions();
  }
  setWindowTitle(tr("%1[*] - %2").arg(shownName).arg(tr("SCIRun")));
}

QString SCIRunMainWindow::strippedName(const QString& fullFileName)
{
  QFileInfo info(fullFileName);
  return info.fileName();
}

void SCIRunMainWindow::updateRecentFileActions()
{
  QMutableStringListIterator i(recentFiles_);
  while (i.hasNext()) {
    if (!QFile::exists(i.next()))
      i.remove();
  }

  for (int j = 0; j < MaxRecentFiles; ++j)
  {
    if (j < recentFiles_.count())
    {
      auto text = tr("&%1 %2")
        .arg(j + 1)
        .arg(strippedName(recentFiles_[j]));

      recentFileActions_[j]->setText(text);
      recentFileActions_[j]->setData(recentFiles_[j]);
      recentFileActions_[j]->setVisible(true);
    }
    else
    {
      recentFileActions_[j]->setVisible(false);
    }
  }
}

void SCIRunMainWindow::loadRecentNetwork()
{
  if (okToContinue())
  {
    auto action = qobject_cast<QAction*>(sender());
    if (action)
      loadNetworkFile(action->data().toString());
  }
}

void SCIRunMainWindow::closeEvent(QCloseEvent* event)
{
  windowState_ = saveState();
  if (okToContinue())
  {
    writeSettings();
    event->accept();
  }
  else
    event->ignore();
}

bool SCIRunMainWindow::okToContinue()
{
  if (isWindowModified()
		&& !Application::Instance().parameters()->isRegressionMode()
		&& !quitAfterExecute_
		&& !skipSaveCheck_)
  {
    int r = QMessageBox::warning(this, tr("SCIRun 5"), tr("The document has been modified.\n" "Do you want to save your changes?"),
      QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
    if (QMessageBox::Yes == r)
    {
      saveNetwork();
      return true;
    }
    else if (QMessageBox::Cancel == r)
      return false;
  }
  return true;
}

//TODO: hook up to modules' state_changed_sig_t via GlobalStateManager
//TODO: pass a boolean here to avoid updating total modules when only connections are made--saves a lock
void SCIRunMainWindow::networkModified()
{
  setWindowModified(true);
  networkProgressBar_->updateTotalModules(networkEditor_->numModules());
}

QString SCIRunMainWindow::mostRecentFile() const
{
  return !recentFiles_.empty() ? recentFiles_[0] : "";
}

void SCIRunMainWindow::setActionIcons()
{
  actionNew_->setIcon(QPixmap(":/general/Resources/new/general/new.png"));
  actionLoad_->setIcon(QPixmap(":/general/Resources/new/general/folder.png"));
  actionSave_->setIcon(QPixmap(":/general/Resources/new/general/save.png"));
  actionRunScript_->setIcon(QPixmap(":/general/Resources/new/general/wand.png"));
  actionExecuteAll_->setIcon(QPixmap(":/general/Resources/new/general/run.png"));
  actionUndo_->setIcon(QPixmap(":/general/Resources/undo.png"));
  actionRedo_->setIcon(QPixmap(":/general/Resources/redo.png"));

  actionHideAllModuleUIs_->setIcon(QPixmap(":/general/Resources/new/general/hideAll.png"));
  actionPinAllModuleUIs_->setIcon(QPixmap(":/general/Resources/new/general/rightAll.png"));
  actionRestoreAllModuleUIs_->setIcon(QPixmap(":/general/Resources/new/general/showAll.png"));

  actionCenterNetworkViewer_->setIcon(QPixmap(":/general/Resources/align_center.png"));
  actionResetNetworkZoom_->setIcon(QPixmap(":/general/Resources/zoom_reset.png"));
  actionZoomIn_->setIcon(QPixmap(":/general/Resources/zoom_in.png"));
  actionZoomOut_->setIcon(QPixmap(":/general/Resources/zoom_out.png"));
  actionZoomBestFit_->setIcon(QPixmap(":/general/Resources/zoom_fit.png"));
  actionDragMode_->setIcon(QPixmap(":/general/Resources/cursor_hand_icon.png"));
  actionSelectMode_->setIcon(QPixmap(":/general/Resources/select.png"));

  actionToggleMetadataLayer_->setIcon(QPixmap(":/general/Resources/metadataLayer.png"));
  actionToggleTagLayer_->setIcon(QPixmap(":/general/Resources/tagLayer.png"));
  actionMakeSubnetwork_->setIcon(QPixmap(":/general/Resources/subnet3.png"));
  //IBBM disable
  actionMakeSubnetwork_->setDisabled(true);
}

void SCIRunMainWindow::filterModuleNamesInTreeView(const QString& start)
{
  ShowAll show;
  visitTree(moduleSelectorTreeWidget_, show);

  bool regexSelected = filterActionGroup_->checkedAction()->text().contains("wildcards");

  HideItemsNotMatchingString func(regexSelected, start);

  //note: goofy double call, first to hide the leaves, then hide the categories.
  visitTree(moduleSelectorTreeWidget_, func);
  visitTree(moduleSelectorTreeWidget_, func);
}

void SCIRunMainWindow::makeFilterButtonMenu()
{
  auto filterMenu = new QMenu(filterButton_);
  filterActionGroup_ = new QActionGroup(filterMenu);
  auto startsWithAction = new QAction("Starts with", filterButton_);
  startsWithAction->setCheckable(true);
  filterActionGroup_->addAction(startsWithAction);
  filterMenu->addAction(startsWithAction);

  auto wildcardAction = new QAction("Use wildcards", filterButton_);
  wildcardAction->setCheckable(true);
  filterActionGroup_->addAction(wildcardAction);
  wildcardAction->setChecked(true);
  filterMenu->addAction(wildcardAction);

  filterButton_->setMenu(filterMenu);
}

void SCIRunMainWindow::makePipesCubicBezier()
{
  networkEditor_->setConnectionPipelineType(static_cast<int>(ConnectionDrawType::CUBIC));
}

void SCIRunMainWindow::makePipesEuclidean()
{
  networkEditor_->setConnectionPipelineType(static_cast<int>(ConnectionDrawType::EUCLIDEAN));
}

void SCIRunMainWindow::makePipesManhattan()
{
  networkEditor_->setConnectionPipelineType(static_cast<int>(ConnectionDrawType::MANHATTAN));
}

void SCIRunMainWindow::setConnectionPipelineType(int type)
{
	networkEditor_->setConnectionPipelineType(type);
  switch (ConnectionDrawType(type))
	{
  case ConnectionDrawType::MANHATTAN:
		prefsWindow_->manhattanPipesRadioButton_->setChecked(true);
		break;
  case ConnectionDrawType::CUBIC:
		prefsWindow_->cubicPipesRadioButton_->setChecked(true);
		break;
  case ConnectionDrawType::EUCLIDEAN:
		prefsWindow_->euclideanPipesRadioButton_->setChecked(true);
		break;
	}

  {
    QSettings settings;
    settings.setValue("connectionPipeType", networkEditor_->connectionPipelineType());
  }
}

void SCIRunMainWindow::setSaveBeforeExecute(int state)
{
  prefsWindow_->setSaveBeforeExecute(state != 0);
}

void SCIRunMainWindow::setDragMode(bool toggle)
{
  if (toggle)
  {
    networkEditor_->setMouseAsDragMode();
    statusBar()->showMessage("Mouse in drag mode", 2000);
  }
  actionSelectMode_->setChecked(!actionDragMode_->isChecked());
}

void SCIRunMainWindow::setSelectMode(bool toggle)
{
  if (toggle)
  {
    networkEditor_->setMouseAsSelectMode();
    statusBar()->showMessage("Mouse in select mode", 2000);
  }
  actionDragMode_->setChecked(!actionSelectMode_->isChecked());
}

void SCIRunMainWindow::switchMouseMode()
{
  if (actionDragMode_->isChecked())
  {
    setSelectMode(true);
    actionSelectMode_->setChecked(true);
  }
  else // select->drag
  {
    setDragMode(true);
    actionDragMode_->setChecked(true);
  }
}

void SCIRunMainWindow::zoomNetwork()
{
  auto action = qobject_cast<QAction*>(sender());
  if (action)
  {
    const auto name = action->text();
    if (name == "Zoom In")
    {
      networkEditor_->zoomIn();
    }
    else if (name == "Zoom Out")
    {
      networkEditor_->zoomOut();
    }
    else if (name == "Reset Network Zoom")
    {
      networkEditor_->zoomReset();
    }
    else if (name == "Zoom Best Fit")
    {
      networkEditor_->zoomBestFit();
    }
  }
  else
  {
    qDebug() << "Sender was null or not an action";
  }
}

void SCIRunMainWindow::showZoomStatusMessage(int zoomLevel)
{
  statusBar()->showMessage(tr("Zoom: %1%").arg(zoomLevel), 2000);
}

void SCIRunMainWindow::setupScriptedEventsWindow()
{
  triggeredEventsWindow_ = new TriggeredEventsWindow(this);
  connect(actionTriggeredEvents_, SIGNAL(toggled(bool)), triggeredEventsWindow_, SLOT(setVisible(bool)));
  connect(triggeredEventsWindow_, SIGNAL(visibilityChanged(bool)), actionTriggeredEvents_, SLOT(setChecked(bool)));
  triggeredEventsWindow_->hide();
}

void SCIRunMainWindow::setupProvenanceWindow()
{
  ProvenanceManagerHandle provenanceManager(new ProvenanceManager<NetworkFileHandle>(networkEditor_));
  provenanceWindow_ = new ProvenanceWindow(provenanceManager, this);
  connect(actionProvenance_, SIGNAL(toggled(bool)), provenanceWindow_, SLOT(setVisible(bool)));
  connect(provenanceWindow_, SIGNAL(visibilityChanged(bool)), actionProvenance_, SLOT(setChecked(bool)));

  connect(actionUndo_, SIGNAL(triggered()), provenanceWindow_, SLOT(undo()));
  connect(actionRedo_, SIGNAL(triggered()), provenanceWindow_, SLOT(redo()));
  actionUndo_->setEnabled(false);
  actionRedo_->setEnabled(false);
  connect(provenanceWindow_, SIGNAL(undoStateChanged(bool)), actionUndo_, SLOT(setEnabled(bool)));
  connect(provenanceWindow_, SIGNAL(redoStateChanged(bool)), actionRedo_, SLOT(setEnabled(bool)));
  connect(provenanceWindow_, SIGNAL(networkModified()), networkEditor_, SLOT(updateViewport()));

  commandConverter_.reset(new GuiActionProvenanceConverter(networkEditor_));

  connect(commandConverter_.get(), SIGNAL(provenanceItemCreated(SCIRun::Dataflow::Engine::ProvenanceItemHandle)), provenanceWindow_, SLOT(addProvenanceItem(SCIRun::Dataflow::Engine::ProvenanceItemHandle)));

	provenanceWindow_->hide();
}

void SCIRunMainWindow::filterDoubleClickedModuleSelectorItem(QTreeWidgetItem* item)
{
  if (item && item->childCount() == 0)
    Q_EMIT moduleItemDoubleClicked();
}

void SCIRunMainWindow::setupDevConsole()
{
  devConsole_ = new DeveloperConsole(this);
  connect(actionDevConsole_, SIGNAL(toggled(bool)), devConsole_, SLOT(setVisible(bool)));
  connect(devConsole_, SIGNAL(visibilityChanged(bool)), actionDevConsole_, SLOT(setChecked(bool)));
  devConsole_->setVisible(false);
  devConsole_->setFloating(true);
  addDockWidget(Qt::TopDockWidgetArea, devConsole_);
  actionDevConsole_->setShortcut(QKeySequence("`"));
  connect(devConsole_, SIGNAL(executorChosen(int)), this, SLOT(setExecutor(int)));
  connect(devConsole_, SIGNAL(globalPortCachingChanged(bool)), this, SLOT(setGlobalPortCaching(bool)));
  //NetworkEditor::setViewUpdateFunc([this](const QString& s) { devConsole_->updateNetworkViewLog(s); });
}

void SCIRunMainWindow::setExecutor(int type)
{
  LOG_DEBUG("Executor of type " << type << " selected"  << std::endl);
  networkEditor_->getNetworkEditorController()->setExecutorType(type);
}

void SCIRunMainWindow::setGlobalPortCaching(bool enable)
{
  LOG_DEBUG("Global port caching flag set to " << (enable ? "true" : "false") << std::endl);
  //TODO: encapsulate better
  SimpleSink::setGlobalPortCachingFlag(enable);
}

void SCIRunMainWindow::readDefaultNotePosition(int index)
{
  Q_EMIT defaultNotePositionChanged(defaultNotePositionGetter_->position()); //TODO: unit test.
}

void SCIRunMainWindow::readDefaultNoteSize(int index)
{
  Q_EMIT defaultNoteSizeChanged(defaultNotePositionGetter_->size()); //TODO: unit test.
}

void SCIRunMainWindow::setupPreferencesWindow()
{
  prefsWindow_ = new PreferencesWindow(networkEditor_, [this]() { writeSettings(); }, this);

  connect(actionPreferences_, SIGNAL(triggered()), prefsWindow_, SLOT(show()));

  prefsWindow_->setVisible(false);
}

void SCIRunMainWindow::setupPythonConsole()
{
#ifdef BUILD_WITH_PYTHON
  pythonConsole_ = new PythonConsoleWidget(networkEditor_, this);
  connect(actionPythonConsole_, SIGNAL(toggled(bool)), pythonConsole_, SLOT(setVisible(bool)));
  actionPythonConsole_->setIcon(QPixmap(":/general/Resources/terminal.png"));
  connect(pythonConsole_, SIGNAL(visibilityChanged(bool)), actionPythonConsole_, SLOT(setChecked(bool)));
  pythonConsole_->setVisible(false);
  pythonConsole_->setFloating(true);
	pythonConsole_->setObjectName("PythonConsole");
  addDockWidget(Qt::TopDockWidgetArea, pythonConsole_);
#else
  actionPythonConsole_->setEnabled(false);
#endif
}

void SCIRunMainWindow::runPythonScript(const QString& scriptFileName)
{
#ifdef BUILD_WITH_PYTHON
  NetworkEditor::InEditingContext iec(networkEditor_);
  GuiLogger::Instance().logInfo("RUNNING PYTHON SCRIPT: " + scriptFileName);
  PythonInterpreter::Instance().importSCIRunLibrary();
  PythonInterpreter::Instance().run_file(scriptFileName.toStdString());
  statusBar()->showMessage(tr("Script is running."), 2000);
#else
  GuiLogger::Instance().logInfo("Python not included in this build, cannot run " + scriptFileName);
#endif
}

void SCIRunMainWindow::runScript()
{
  if (okToContinue())
  {
    QString filename = QFileDialog::getOpenFileName(this, "Load Script...", latestNetworkDirectory_.path(), "*.py");
    runPythonScript(filename);
  }
}

namespace {

  QColor favesColor()
  {
    return SCIRunMainWindow::Instance()->newInterface() ? Qt::yellow : Qt::darkYellow;
  }
  QColor packageColor()
  {
    return SCIRunMainWindow::Instance()->newInterface() ? Qt::yellow : Qt::darkYellow;
  }
  QColor categoryColor()
  {
    return SCIRunMainWindow::Instance()->newInterface() ? Qt::green : Qt::darkGreen;
  }

  const QString bullet = "* ";
  const QString hash = "# ";
  const QString favoritesText = bullet + "Favorites";
  const QString clipboardHistoryText = hash + "Clipboard History";
  const QString savedSubsText = hash + "Saved Fragments";

  void addFavoriteMenu(QTreeWidget* tree)
  {
    auto faves = new QTreeWidgetItem();
    faves->setText(0, favoritesText);
    faves->setForeground(0, favesColor());

    tree->addTopLevelItem(faves);
  }

  QTreeWidgetItem* getTreeMenu(QTreeWidget* tree, const QString& text)
  {
    for (int i = 0; i < tree->topLevelItemCount(); ++i)
    {
      auto top = tree->topLevelItem(i);
      if (top->text(0) == text)
      {
        return top;
      }
    }
    return nullptr;
  }

  QTreeWidgetItem* getFavoriteMenu(QTreeWidget* tree)
  {
    return getTreeMenu(tree, favoritesText);
  }

  QTreeWidgetItem* getClipboardHistoryMenu(QTreeWidget* tree)
  {
    return getTreeMenu(tree, clipboardHistoryText);
  }

  QTreeWidgetItem* getSavedSubnetworksMenu(QTreeWidget* tree)
  {
    return getTreeMenu(tree, savedSubsText);
  }

  void addSnippet(const QString& code, QTreeWidgetItem* snips)
  {
    auto snipItem = new QTreeWidgetItem();
    snipItem->setText(0, code);
    snips->addChild(snipItem);
  }

  void readCustomSnippets(QTreeWidgetItem* snips)
  {
    QFile inputFile("patterns.txt");
    if (inputFile.open(QIODevice::ReadOnly))
    {
      GuiLogger::Instance().logInfo("Pattern file opened: " + inputFile.fileName());
      QTextStream in(&inputFile);
      while (!in.atEnd())
      {
        QString line = in.readLine();
        addSnippet(line, snips);
        GuiLogger::Instance().logInfo("Pattern read: " + line);
      }
      inputFile.close();
    }
  }

  void addSnippetMenu(QTreeWidget* tree)
	{
		auto snips = new QTreeWidgetItem();
    snips->setText(0, bullet + "Typical Patterns");
		snips->setForeground(0, favesColor());

		//hard-code a few popular ones.

    addSnippet("[ReadField*->ShowField->ViewScene]", snips);
    addSnippet("[CreateLatVol->ShowField->ViewScene]", snips);
    addSnippet("[ReadField*->ReportFieldInfo]", snips);
    addSnippet("[ReadMatrix*->ReportMatrixInfo]", snips);
    addSnippet("[CreateStandardColorMap->RescaleColorMap->ShowField->ViewScene]", snips);
    addSnippet("[GetFieldBoundary->FairMesh->ShowField]", snips);

	  readCustomSnippets(snips);

	  tree->addTopLevelItem(snips);
	}
  static const QString saveFragmentData("fragmentTree");

  void addSavedSubnetworkMenu(QTreeWidget* tree)
  {
    auto savedSubnetworks = new QTreeWidgetItem();
    savedSubnetworks->setText(0, savedSubsText);
    savedSubnetworks->setData(0, Qt::UserRole, saveFragmentData);
    savedSubnetworks->setForeground(0, favesColor());
    tree->addTopLevelItem(savedSubnetworks);
  }

  void addClipboardHistoryMenu(QTreeWidget* tree)
  {
    auto clips = new QTreeWidgetItem();
    clips->setText(0, clipboardHistoryText);
    clips->setForeground(0, favesColor());
    tree->addTopLevelItem(clips);
  }

  QTreeWidgetItem* addFavoriteItem(QTreeWidgetItem* faves, QTreeWidgetItem* module)
  {
    LOG_DEBUG("Adding item to favorites: " << module->text(0).toStdString() << std::endl);
    auto copy = new QTreeWidgetItem(*module);
    copy->setData(0, Qt::CheckStateRole, QVariant());
    if (copy->textColor(0) == CLIPBOARD_COLOR)
    {
      copy->setFlags(copy->flags() | Qt::ItemIsEditable);
    }
    faves->addChild(copy);
    return copy;
  }

  void fillTreeWidget(QTreeWidget* tree, const ModuleDescriptionMap& moduleMap, const QStringList& favoriteModuleNames)
  {
    auto faves = getFavoriteMenu(tree);
		for (const auto& package : moduleMap)
    {
      const auto& packageName = package.first;
      auto packageItem = new QTreeWidgetItem();
      packageItem->setText(0, QString::fromStdString(packageName));
      packageItem->setForeground(0, packageColor());
      tree->addTopLevelItem(packageItem);
      size_t totalModules = 0;
      for (const auto& category : package.second)
      {
        const auto& categoryName = category.first;
        auto categoryItem = new QTreeWidgetItem();
        categoryItem->setText(0, QString::fromStdString(categoryName));
        categoryItem->setForeground(0, categoryColor());
        packageItem->addChild(categoryItem);
				for (const auto& module : category.second)
        {
          const auto& moduleName = module.first;
          auto moduleItem = new QTreeWidgetItem();
          auto name = QString::fromStdString(moduleName);
          moduleItem->setText(0, name);
          if (favoriteModuleNames.contains(name))
          {
            moduleItem->setCheckState(0, Qt::Checked);
            addFavoriteItem(faves, moduleItem);
          }
          else
          {
            moduleItem->setCheckState(0, Qt::Unchecked);
          }
          moduleItem->setText(1, QString::fromStdString(module.second.moduleStatus_));
          moduleItem->setForeground(1, Qt::lightGray);
          moduleItem->setText(2, QString::fromStdString(module.second.moduleInfo_));
          moduleItem->setForeground(2, Qt::lightGray);
          categoryItem->addChild(moduleItem);
          totalModules++;
        }
        categoryItem->setText(1, "Category Module Count = " + QString::number(category.second.size()));
        categoryItem->setForeground(1, Qt::magenta);
      }
      packageItem->setText(1, "Package Module Count = " + QString::number(totalModules));
      packageItem->setForeground(1, Qt::magenta);
    }
  }

  void sortFavorites(QTreeWidget* tree)
  {
    auto faves = getFavoriteMenu(tree);
    faves->sortChildren(0, Qt::AscendingOrder);
  }

}

void SCIRunMainWindow::showModuleSelectorContextMenu(const QPoint& pos)
{
  auto globalPos = moduleSelectorTreeWidget_->mapToGlobal(pos);
	auto item = moduleSelectorTreeWidget_->selectedItems()[0];
	auto subnetData = item->data(0, Qt::UserRole).toString();
  if (saveFragmentData == subnetData)
  {
    QMenu menu;
		menu.addAction("Export fragment list...", this, SLOT(exportFragmentList()));
    menu.addAction("Import fragment list...", this, SLOT(importFragmentList()));
    menu.addAction("Clear", this, SLOT(clearFragmentList()));
  	menu.exec(globalPos);
  }
	else if (!subnetData.isEmpty())
	{
  	QMenu menu;
		menu.addAction("Rename", this, SLOT(renameSavedSubnetwork()))->setProperty("ID", subnetData);
		menu.addAction("Delete", this, SLOT(removeSavedSubnetwork()))->setProperty("ID", subnetData);
  	menu.exec(globalPos);
	}
}

void SCIRunMainWindow::clearFragmentList()
{
  savedSubnetworksNames_.clear();
  savedSubnetworksXml_.clear();
  auto menu = getSavedSubnetworksMenu(moduleSelectorTreeWidget_);
  auto count = menu->childCount();
  for (int i = 0; i < count; ++i)
  {
    delete menu->child(0);
  }
}

using NetworkFragmentXMLMap = std::map<std::string, std::pair<std::string, std::string>>;

class NetworkFragmentXML
{
public:
  NetworkFragmentXMLMap fragments;
private:
  friend class boost::serialization::access;
  template <class Archive>
  void serialize(Archive& ar, const unsigned int version)
  {
    ar & BOOST_SERIALIZATION_NVP(fragments);
  }
};

void SCIRunMainWindow::importFragmentList()
{
  auto filename = QFileDialog::getOpenFileName(this, "Import Network Fragments...", latestNetworkDirectory_.path(), "*.srn5fragment");
  if (!filename.isEmpty())
  {
    auto frags = XMLSerializer::load_xml<NetworkFragmentXML>(filename.toStdString());
    QMap<QString, QVariant> names, xmls;
    for (const auto& frag : frags->fragments)
    {
      auto key = QString::fromStdString(frag.first);
      names.insert(key, QString::fromStdString(frag.second.first));
      xmls.insert(key, QString::fromStdString(frag.second.second));
    }
    addFragmentsToMenu(names, xmls);
    savedSubnetworksNames_.unite(names);
    savedSubnetworksXml_.unite(xmls);
    showStatusMessage("Fragment list imported: " + filename, 2000);
  }
}

void SCIRunMainWindow::exportFragmentList()
{
  auto filename = QFileDialog::getSaveFileName(this, "Export Network Fragments...", latestNetworkDirectory_.path(), "*.srn5fragment");
  if (!filename.isEmpty())
  {
    NetworkFragmentXML data;
    auto keys = savedSubnetworksNames_.keys();
    for (auto&& tup : zip(savedSubnetworksNames_, savedSubnetworksXml_, keys))
    {
      QVariant name, xml;
      QString key;
      boost::tie(name, xml, key) = tup;
      data.fragments[key.toStdString()] = { name.toString().toStdString(), xml.toString().toStdString() };
    }
    XMLSerializer::save_xml(data, filename.toStdString(), "fragments");
    showStatusMessage("Fragment list exported: " + filename, 2000);
  }
}

void SCIRunMainWindow::fillSavedSubnetworkMenu()
{
  if (savedSubnetworksNames_.size() != savedSubnetworksXml_.size())
  {
    qDebug() << "invalid subnet saved settings: sizes don't match" << savedSubnetworksNames_.size() << "," << savedSubnetworksXml_.size() << ',' << savedSubnetworksNames_.keys().size() << savedSubnetworksNames_.keys();
    return;
  }
  addFragmentsToMenu(savedSubnetworksNames_, savedSubnetworksXml_);
}

void SCIRunMainWindow::addFragmentsToMenu(const QMap<QString, QVariant>& names, const QMap<QString, QVariant>& xmls)
{
  auto savedSubnetworks = getSavedSubnetworksMenu(moduleSelectorTreeWidget_);
  auto keys = names.keys(); // don't inline this into the zip call! temporary containers don't work with zip.
  for (auto&& tup : zip(names, xmls, keys))
  {
    auto subnet = new QTreeWidgetItem();
    QVariant name, xml;
    QString key;
    boost::tie(name, xml, key) = tup;
    subnet->setText(0, name.toString());
    subnet->setData(0, clipboardKey, xml.toString());
		subnet->setTextColor(0, CLIPBOARD_COLOR);
		savedSubnetworks->addChild(subnet);
		setupSubnetItem(subnet, false, key);
  }
}

void SCIRunMainWindow::fillModuleSelector()
{
  moduleSelectorTreeWidget_->clear();

  auto moduleDescs = networkEditor_->getNetworkEditorController()->getAllAvailableModuleDescriptions();

  addFavoriteMenu(moduleSelectorTreeWidget_);
	addSnippetMenu(moduleSelectorTreeWidget_);
	addSavedSubnetworkMenu(moduleSelectorTreeWidget_);
  fillSavedSubnetworkMenu();
	addClipboardHistoryMenu(moduleSelectorTreeWidget_);
  fillTreeWidget(moduleSelectorTreeWidget_, moduleDescs, favoriteModuleNames_);
  sortFavorites(moduleSelectorTreeWidget_);

  GrabNameAndSetFlags visitor;
  visitTree(moduleSelectorTreeWidget_, visitor);

  moduleSelectorTreeWidget_->expandAll();
  moduleSelectorTreeWidget_->resizeColumnToContents(0);
  moduleSelectorTreeWidget_->resizeColumnToContents(1);
  moduleSelectorTreeWidget_->sortByColumn(0, Qt::AscendingOrder);

  connect(moduleSelectorTreeWidget_, SIGNAL(itemChanged(QTreeWidgetItem*, int)), this, SLOT(handleCheckedModuleEntry(QTreeWidgetItem*, int)));

  moduleSelectorTreeWidget_->setStyleSheet(
    "QTreeWidget::indicator:unchecked {image: url(:/general/Resources/faveNo.png);}"
    "QTreeWidget::indicator:checked {image: url(:/general/Resources/faveYes.png);}");
}

template <class T>
QString idFromPointer(T* item)
{
  QString addressString;
  QTextStream addressStream(&addressString);
  addressStream << static_cast<const void*>(item);
  addressStream.flush();
  return addressString;
}

void SCIRunMainWindow::setupSubnetItem(QTreeWidgetItem* fave, bool addToMap, const QString& idFromMap)
{
  auto id = addToMap ? idFromPointer(fave) + "::" + fave->text(0) : idFromMap;
  fave->setData(0, Qt::UserRole, id);

  if (addToMap)
  {
    savedSubnetworksXml_[id] = fave->data(0, clipboardKey).toString();
    savedSubnetworksNames_[id] = fave->text(0);
    fave->setFlags(fave->flags() & ~Qt::ItemIsEditable);
  }
}

void SCIRunMainWindow::handleCheckedModuleEntry(QTreeWidgetItem* item, int column)
{
  if (item && 0 == column)
  {
    moduleSelectorTreeWidget_->setCurrentItem(item);

    auto faves = item->textColor(0) == CLIPBOARD_COLOR ? getSavedSubnetworksMenu(moduleSelectorTreeWidget_) : getFavoriteMenu(moduleSelectorTreeWidget_);

    if (item->checkState(0) == Qt::Checked)
    {
      if (faves)
      {
        auto fave = addFavoriteItem(faves, item);
        faves->sortChildren(0, Qt::AscendingOrder);
        if (item->textColor(0) != CLIPBOARD_COLOR)
          favoriteModuleNames_ << item->text(0);
        else
        {
					setupSubnetItem(fave, true, "");
        }
      }
    }
    else
    {
      if (faves && item->textColor(0) != CLIPBOARD_COLOR)
      {
        favoriteModuleNames_.removeAll(item->text(0));
        for (int i = 0; i < faves->childCount(); ++i)
        {
          auto child = faves->child(i);
          if (child->text(0) == item->text(0))
            faves->removeChild(child);
        }
      }
    }
  }
}

void SCIRunMainWindow::removeSavedSubnetwork()
{
	auto toDelete = sender()->property("ID").toString();
  savedSubnetworksNames_.remove(toDelete);
  savedSubnetworksXml_.remove(toDelete);
	auto tree = getSavedSubnetworksMenu(moduleSelectorTreeWidget_);
	for (int i = 0; i < tree->childCount(); ++i)
	{
		auto subnet = tree->child(i);
		if (toDelete == subnet->data(0, Qt::UserRole).toString())
		{
			delete tree->takeChild(i);
			break;
		}
	}
}

void SCIRunMainWindow::renameSavedSubnetwork()
{
  auto toRename = sender()->property("ID").toString();
  bool ok;
  auto text = QInputDialog::getText(this, tr("Rename fragment"), tr("Enter new fragment name:"), QLineEdit::Normal, savedSubnetworksNames_[toRename].toString(), &ok);
  if (ok && !text.isEmpty())
  {
    savedSubnetworksNames_[toRename] = text;
    auto tree = getSavedSubnetworksMenu(moduleSelectorTreeWidget_);
    for (int i = 0; i < tree->childCount(); ++i)
    {
      auto subnet = tree->child(i);
      if (toRename == subnet->data(0, Qt::UserRole).toString())
      {
				subnet->setText(0, text);
        break;
      }
    }
  }
}

bool SCIRunMainWindow::isInFavorites(const QString& module) const
{
	return favoriteModuleNames_.contains(module);
}

void SCIRunMainWindow::displayAcknowledgement()
{
  QMessageBox::information(this, "NIH/NIGMS Center for Integrative Biomedical Computing Acknowledgment",
    "CIBC software and the data sets provided on this web site are Open Source software projects that are principally funded through the SCI Institute's NIH/NCRR CIBC. For us to secure the funding that allows us to continue providing this software, we must have evidence of its utility. Thus we ask users of our software and data to acknowledge us in their publications and inform us of these publications. Please use the following acknowledgment and send us references to any publications, presentations, or successful funding applications that make use of the NIH/NCRR CIBC software or data sets we provide. <p> <i>This project was supported by the National Institute of General Medical Sciences of the National Institutes of Health under grant number P41GM103545.</i>");
}

void SCIRunMainWindow::helpWithToolkitCreation()
{
  QMessageBox::information(this, "Temp",
    "<b>Help with toolkit creation--for power users</b>"
    "<p> First, gather all network files for your toolkit into a single directory. This directory may contain "
    "one level of subdirectories. Next, "
    "in your build directory (you must SCIRun from source), locate the executable named <code>bundle_toolkit</code>. "
    " The usage is:"
    "<pre>bundle_toolkit OUTPUT_FILE [DIRECTORY_TO_SCAN]</pre>"
    "where OUTPUT_FILE is the desired name of your toolkit bundle. If no directory is specified, the current directory is scanned."
    "<p>For further assistance, visit https://github.com/SCIInstitute/FwdInvToolkit/wiki."
);
}

void SCIRunMainWindow::setDataDirectory(const QString& dir)
{
  if (!dir.isEmpty())
  {
    prefsWindow_->scirunDataLineEdit_->setText(dir);
    prefsWindow_->scirunDataLineEdit_->setToolTip(dir);

    RemembersFileDialogDirectory::setStartingDir(dir);
    Preferences::Instance().setDataDirectory(dir.toStdString());
    Q_EMIT dataDirectorySet(dir);
  }
}

void SCIRunMainWindow::setDataPath(const QString& dirs)
{
	if (!dirs.isEmpty())
	{
    prefsWindow_->scirunDataPathTextEdit_->setPlainText(dirs);
    prefsWindow_->scirunDataPathTextEdit_->setToolTip(dirs);

		Preferences::Instance().setDataPath(dirs.toStdString());
	}
}

void SCIRunMainWindow::addToDataDirectory(const QString& dir)
{
	if (!dir.isEmpty())
	{
    auto text = prefsWindow_->scirunDataPathTextEdit_->toPlainText();
		if (!text.isEmpty())
			text += ";\n";
		text += dir;
    prefsWindow_->scirunDataPathTextEdit_->setPlainText(text);
    prefsWindow_->scirunDataPathTextEdit_->setToolTip(prefsWindow_->scirunDataPathTextEdit_->toPlainText());

		RemembersFileDialogDirectory::setStartingDir(dir);
		Preferences::Instance().addToDataPath(dir.toStdString());
	}
}

void SCIRunMainWindow::setDataDirectoryFromGUI()
{
  auto dir = QFileDialog::getExistingDirectory(this, tr("Choose Data Directory"), QString::fromStdString(Core::Preferences::Instance().dataDirectory().parent_path().string()));
  setDataDirectory(dir);

  {
    QSettings settings;
    settings.setValue("dataDirectory", QString::fromStdString(Preferences::Instance().dataDirectory().string()));
  }
}

void SCIRunMainWindow::addToPathFromGUI()
{
  auto dir = QFileDialog::getExistingDirectory(this, tr("Add Directory to Data Path"), ".");
	addToDataDirectory(dir);
}

void SCIRunMainWindow::reportIssue()
{
  if (QMessageBox::Ok == QMessageBox::information(this, "Report Issue",
    "Click OK to be taken to SCIRun's Github issue reporting page.\n\nFor bug reports, please follow the template.", QMessageBox::Ok|QMessageBox::Cancel))
  {
    QDesktopServices::openUrl(QUrl("https://github.com/SCIInstitute/SCIRun/issues/new", QUrl::TolerantMode));
  }
}

bool SCIRunMainWindow::newInterface() const
{
  return Application::Instance().parameters()->entireCommandLine().find("--originalGUI") == std::string::npos;
}

void SCIRunMainWindow::printStyleSheet() const
{
  std::cout << "Printing style sheet details map" << std::endl;
  for (auto styleMapIt = styleSheetDetails_.constBegin(); styleMapIt != styleSheetDetails_.constEnd(); ++styleMapIt)
  {
    std::cout << "Style for: " << styleMapIt.key().toStdString() << std::endl;
    for (auto styleIt = styleMapIt.value().constBegin(); styleIt != styleMapIt.value().constEnd(); ++styleIt)
    {
      std::cout << "key: " << styleIt.key().toStdString() << " value: " << styleIt.value().toStdString() << std::endl;
    }
  }
}

void SCIRunMainWindow::setFocusOnFilterLine()
{
  moduleFilterLineEdit_->setFocus(Qt::ShortcutFocusReason);
  statusBar()->showMessage(tr("Module filter activated"), 2000);
}

//disable these
void SCIRunMainWindow::addModuleKeyboardAction()
{
  //TODO
  auto item = moduleSelectorTreeWidget_->currentItem();
  if (item && item->childCount() == 0)
    std::cout << "Current module: " << item->text(0).toStdString() << std::endl;
}

void SCIRunMainWindow::selectModuleKeyboardAction()
{
  moduleSelectorTreeWidget_->setFocus(Qt::ShortcutFocusReason);
  statusBar()->showMessage(tr("Module selection activated"), 2000);
}

void SCIRunMainWindow::modulesSnapToChanged()
{
  bool snapTo = prefsWindow_->modulesSnapToCheckBox_->isChecked();
  Preferences::Instance().modulesSnapToGrid.setValue(snapTo);
}

void SCIRunMainWindow::highlightPortsChanged()
{
  bool val = prefsWindow_->portSizeEffectsCheckBox_->isChecked();
  Preferences::Instance().highlightPorts.setValue(val);
}

void SCIRunMainWindow::resetWindowLayout()
{
  configurationDockWidget_->hide();
  devConsole_->hide();
  provenanceWindow_->hide();
  moduleSelectorDockWidget_->show();
  moduleSelectorDockWidget_->setFloating(false);
  addDockWidget(Qt::LeftDockWidgetArea, moduleSelectorDockWidget_);
}

void SCIRunMainWindow::hideNonfunctioningWidgets()
{
	//TODO: make issues to implement these, as I don't want to forget they are there.
  QList<QAction*> nonfunctioningActions;
  nonfunctioningActions <<
    actionInsert_;
  QList<QMenu*> nonfunctioningMenus;
  //nonfunctioningMenus <<  menuSubnets_;
  QList<QWidget*> nonfunctioningWidgets;
  nonfunctioningWidgets <<
    prefsWindow_->scirunNetsLabel_ <<
    prefsWindow_->scirunNetsLineEdit_ <<
    prefsWindow_->scirunNetsPushButton_ <<
    prefsWindow_->userDataLabel_ <<
    prefsWindow_->userDataLineEdit_ <<
    prefsWindow_->userDataPushButton_ <<
    prefsWindow_->dataSetGroupBox_ <<
    networkEditorMiniViewLabel_ <<
    miniviewTextLabel_ <<
    prefsWindow_->scirunDataPathTextEdit_ <<
    prefsWindow_->addToPathButton_;

  Q_FOREACH(QAction* a, nonfunctioningActions)
    a->setVisible(false);
  Q_FOREACH(QMenu* m, nonfunctioningMenus)
    m->menuAction()->setVisible(false);
  Q_FOREACH(QWidget* w, nonfunctioningWidgets)
    w->setVisible(false);
}

void SCIRunMainWindow::launchNewUserWizard()
{
  NewUserWizard wiz(this);
  wiz.exec();
}

void SCIRunMainWindow::adjustModuleDock(int state)
{
  bool dockable = prefsWindow_->dockableModulesCheckBox_->isChecked();
  actionPinAllModuleUIs_->setEnabled(dockable);
  Preferences::Instance().modulesAreDockable.setValueWithSignal(dockable);
}

void SCIRunMainWindow::showEvent(QShowEvent* event)
{
  restoreState(windowState_);
  QMainWindow::showEvent(event);
}

void SCIRunMainWindow::hideEvent(QHideEvent * event)
{
  windowState_ = saveState();
  QMainWindow::hideEvent(event);
}

void SCIRunMainWindow::updateDockWidgetProperties(bool isFloating)
{
  auto dock = qobject_cast<QDockWidget*>(sender());
  if (dock && isFloating)
  {
    dock->setWindowFlags(Qt::Window);
    dock->show();
  }
}

#ifdef __APPLE__
static const Qt::Key MetadataShiftKey = Qt::Key_Meta;
#else
static const Qt::Key MetadataShiftKey = Qt::Key_CapsLock;
#endif

void SCIRunMainWindow::keyPressEvent(QKeyEvent *event)
{
	if (event->key() == Qt::Key_Shift)
	{
		showStatusMessage("Network zoom active");
    networkEditor_->adjustExecuteButtonsToDownstream(true);
	}
  else if (event->key() == MetadataShiftKey)
  {
    networkEditor_->metadataLayer(true);
		showStatusMessage("Metadata layer active");
  }
  else if (event->key() == Qt::Key_Alt)
  {
		if (!actionToggleTagLayer_->isChecked())
		{
	 		networkEditor_->tagLayer(true, NoTag);
			showStatusMessage("Tag layer active: none");
		}
  }
	else if (event->key() == Qt::Key_A)
	{
		if (!actionToggleTagLayer_->isChecked())
		{
    	if (networkEditor_->tagLayerActive())
    	{
      	networkEditor_->tagLayer(true, AllTags);
				showStatusMessage("Tag layer active: All");
    	}
		}
	}
  else if (event->key() == Qt::Key_G && (event->modifiers() & Qt::ShiftModifier))
  {
    if (!actionToggleTagLayer_->isChecked())
    {
      if (networkEditor_->tagLayerActive())
      {
        networkEditor_->tagLayer(true, HideGroups);
        showStatusMessage("Tag layer active: Groups hidden");
      }
    }
  }
	else if (event->key() == Qt::Key_G)
	{
		if (!actionToggleTagLayer_->isChecked())
		{
    	if (networkEditor_->tagLayerActive())
    	{
      	networkEditor_->tagLayer(true, ShowGroups);
				showStatusMessage("Tag layer active: Groups shown");
    	}
		}
	}
	else if (event->key() == Qt::Key_J)
	{
		if (!actionToggleTagLayer_->isChecked())
		{
    	if (networkEditor_->tagLayerActive())
    	{
      	networkEditor_->tagLayer(true, ClearTags);
				showStatusMessage("Tag layer active: selected modules' tags cleared");
    	}
	  }
	}
  else if (event->key() >= Qt::Key_0 && event->key() <= Qt::Key_9)
  {
		if (!actionToggleTagLayer_->isChecked())
		{
    	if (networkEditor_->tagLayerActive())
    	{
      	auto key = event->key() - Qt::Key_0;
      	networkEditor_->tagLayer(true, key);
				showStatusMessage("Tag layer active: " + QString::number(key));
    	}
		}
  }
  else if (event->key() == Qt::Key_Period)
	{
    switchMouseMode();
	}

  QMainWindow::keyPressEvent(event);
}

void SCIRunMainWindow::keyReleaseEvent(QKeyEvent *event)
{
	if (event->key() == Qt::Key_Shift)
	{
		showStatusMessage("Network zoom inactive", 1000);
    networkEditor_->adjustExecuteButtonsToDownstream(false);
	}
  else if (event->key() == MetadataShiftKey)
  {
    networkEditor_->metadataLayer(false);
		showStatusMessage("Metadata layer inactive", 1000);
  }
  else if (event->key() == Qt::Key_Alt)
  {
		if (!actionToggleTagLayer_->isChecked())
		{
    	networkEditor_->tagLayer(false, -1);
			showStatusMessage("Tag layer inactive", 1000);
		}
  }

  QMainWindow::keyPressEvent(event);
}

void SCIRunMainWindow::changeExecuteActionIconToStop()
{
  actionExecuteAll_->setIcon(QApplication::style()->standardIcon(QStyle::SP_MediaStop));
	actionExecuteAll_->setText("Halt Execution");
}

void SCIRunMainWindow::changeExecuteActionIconToPlay()
{
  actionExecuteAll_->setIcon(QPixmap(":/general/Resources/new/general/run.png"));
	actionExecuteAll_->setText("Execute All");
}

void SCIRunMainWindow::openLogFolder()
{
  auto logPath = QString::fromStdString(Application::Instance().logDirectory().string());
  QDesktopServices::openUrl(QUrl::fromLocalFile(logPath));
}

void SCIRunMainWindow::adjustExecuteButtonAppearance()
{
  switch (prefsWindow_->actionTextIconCheckBox_->checkState())
  {
  case 0:
    prefsWindow_->actionTextIconCheckBox_->setText("Execute Button Text");
		executeButton_->setToolButtonStyle(Qt::ToolButtonTextOnly);
    break;
  case 1:
    prefsWindow_->actionTextIconCheckBox_->setText("Execute Button Icon");
		executeButton_->setToolButtonStyle(Qt::ToolButtonIconOnly);
    break;
  case 2:
    prefsWindow_->actionTextIconCheckBox_->setText("Execute Button Text+Icon");
		executeButton_->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    break;
  }
}

void SCIRunMainWindow::alertForNetworkCycles(int code)
{
  if (code == -1)
  {
    QMessageBox::warning(this, "Network graph has a cycle", "Your network contains a cycle. The execution scheduler cannot handle cycles at this time. Please ensure all cycles are broken before executing.");
    networkEditor_->resetNetworkDueToCycle();
  }
}

void SCIRunMainWindow::addModuleToWindowList(const QString& modId, bool hasUI)
{
  if (menuCurrent_->actions().isEmpty())
    menuCurrent_->setEnabled(true);
  auto modAction = new QAction(this);
  modAction->setText(modId);
  modAction->setEnabled(hasUI);
  connect(modAction, SIGNAL(triggered()), networkEditor_, SLOT(moduleWindowAction()));
  currentModuleActions_.insert(modId, modAction);
  menuCurrent_->addAction(modAction);

  if (modId.contains("Subnet"))
  {
    if (menuCurrentSubnets_->actions().isEmpty())
      menuCurrentSubnets_->setEnabled(true);

    auto subnetMenu = new QMenu(modId, this);
    auto showAction = new QAction(subnetMenu);
    showAction->setText("Show");
    subnetMenu->addAction(showAction);
    auto renameAction = new QAction(subnetMenu);
    renameAction->setText("Rename...");
    subnetMenu->addAction(renameAction);

    connect(showAction, SIGNAL(triggered()), networkEditor_, SLOT(subnetMenuActionTriggered()));
    connect(renameAction, SIGNAL(triggered()), networkEditor_, SLOT(subnetMenuActionTriggered()));
    currentSubnetActions_.insert(modId, subnetMenu);
    menuCurrentSubnets_->addMenu(subnetMenu);
  }
}

void SCIRunMainWindow::removeModuleFromWindowList(const ModuleId& modId)
{
  auto name = QString::fromStdString(modId.id_);
  auto action = currentModuleActions_[name];
  menuCurrent_->removeAction(action);
  currentModuleActions_.remove(name);
  if (menuCurrent_->actions().isEmpty())
    menuCurrent_->setEnabled(false);

  if (modId.id_.find("Subnet") != std::string::npos)
  {
    auto subnet = currentSubnetActions_[name];
    if (subnet)
      menuCurrentSubnets_->removeAction(subnet->menuAction());
    currentSubnetActions_.remove(name);
    if (menuCurrentSubnets_->actions().isEmpty())
      menuCurrentSubnets_->setEnabled(false);
  }

}

void SCIRunMainWindow::setupTagManagerWindow()
{
  tagManagerWindow_ = new TagManagerWindow(this);
  connect(actionTagManager_, SIGNAL(toggled(bool)), tagManagerWindow_, SLOT(setVisible(bool)));
  connect(tagManagerWindow_, SIGNAL(visibilityChanged(bool)), actionTagManager_, SLOT(setChecked(bool)));
  tagManagerWindow_->hide();
}

void SCIRunMainWindow::showTagHelp()
{
	TagManagerWindow::showHelp(this);
}

void SCIRunMainWindow::toggleTagLayer(bool toggle)
{
	networkEditor_->tagLayer(toggle, AllTags);
	if (toggle)
		showStatusMessage("Tag layer active: all");
	else
		showStatusMessage("Tag layer inactive", 1000);
}

void SCIRunMainWindow::showStatusMessage(const QString& str)
{
	statusBar()->showMessage(str);
}

void SCIRunMainWindow::showStatusMessage(const QString& str, int timeInMsec)
{
	statusBar()->showMessage(str, timeInMsec);
}

void SCIRunMainWindow::showExtendedDataInfo()
{
}

void SCIRunMainWindow::toggleMetadataLayer(bool toggle)
{
	networkEditor_->metadataLayer(toggle);
	//TODO: extract methods
	if (toggle)
		showStatusMessage("Metadata layer active");
	else
		showStatusMessage("Metadata layer inactive", 1000);
}

void SCIRunMainWindow::showKeyboardShortcutsDialog()
{
  if (!shortcuts_)
  {
    shortcuts_ = new ShortcutsInterface(this);
  }
  shortcuts_->show();
}

void SCIRunMainWindow::runNewModuleWizard()
{
  auto wizard = new ClassWizard(this);
	wizard->show();
}

void SCIRunMainWindow::setupVersionButton()
{
  auto qVersion = QString::fromStdString(VersionInfo::GIT_VERSION_TAG);
  versionButton_ = new QPushButton("Version: " + qVersion);
  versionButton_->setFlat(true);
  versionButton_->setToolTip("Click to copy version tag to clipboard");
  versionButton_->setStyleSheet("QToolTip { color: #ffffff; background - color: #2a82da; border: 1px solid white; }");
  connect(versionButton_, SIGNAL(clicked()), this, SLOT(copyVersionToClipboard()));
  statusBar()->addPermanentWidget(versionButton_);
}

void SCIRunMainWindow::copyVersionToClipboard()
{
  QApplication::clipboard()->setText(QString::fromStdString(VersionInfo::GIT_VERSION_TAG));
  statusBar()->showMessage("Version string copied to clipboard.", 2000);
}

void SCIRunMainWindow::updateClipboardHistory(const QString& xml)
{
  auto clips = getClipboardHistoryMenu(moduleSelectorTreeWidget_);

  auto clip = new QTreeWidgetItem();
  clip->setText(0, "clipboard " + QDateTime::currentDateTime().toString("ddd MMMM d yyyy hh:mm:ss.zzz"));
  clip->setToolTip(0, "todo: xml translation");
  clip->setData(0, clipboardKey, xml);
  clip->setTextColor(0, CLIPBOARD_COLOR);

  const int clipMax = 5;
  if (clips->childCount() == clipMax)
    clips->removeChild(clips->child(0));

  clip->setCheckState(0, Qt::Unchecked);
  clips->addChild(clip);
}

void SCIRunMainWindow::showSnippetHelp()
{
  QMessageBox::information(this, "Patterns",
    "Patterns are strings that encode a linear subnetwork. They can vastly shorten network construction time. They take the form [A->B->...->C] where A, B, C, etc are module names, and the arrow represents a connection between adjacent modules. "
    "\n\nThey are available in the module selector and work just like the single module entries there: double-click or drag onto the "
    "network editor to insert the entire snippet. A '*' at the end of the module name will open the UI for that module.\n\nCustom patterns can be created by editing the file patterns.txt (if not present, create it) in the same folder as the SCIRun executable. Enter one pattern per line in the prescribed format, then restart SCIRun for them to appear."
    "\n\nFor feedback, please comment on this issue: https://github.com/SCIInstitute/SCIRun/issues/1263"
    );
}

void SCIRunMainWindow::showClipboardHelp()
{
  QMessageBox::information(this, "Clipboard",
    "The network editor clipboard works on arbitrary network selections (modules and connections). A history of five copied items is kept under \"Clipboard History\" in the module selector. "
    "\n\nTo cut/copy/paste, see the Edit menu and the corresponding hotkeys."
    "\n\nClipboard history items can be starred like module favorites. When starred, they are saved as fragments under \"Saved Fragments,\" which are preserved in application settings. "
    "\n\nThe user may edit the text of the saved fragment items to give them informative names, which are also saved. Hover over them to see a tooltip representation of the saved fragment."
    "\n\nRight-click on the fragment item to rename or delete it."
    );
}

void SCIRunMainWindow::loadPythonAPIDoc()
{
  openPythonAPIDoc();
}

void SCIRunMainWindow::showTriggerHelp()
{
	QMessageBox::information(this, "Triggered Scripts",
    "The triggered scripts interface allows the user to inject Python code that executes whenever a specific event happens. Currently the available events are post-module-add (manually, not "
    "via network loading), and post-network-load (after user loads a file)."
    "\n\nExamples can be found in the GUI when you first load the dialog. The scripts are saved at the application level and can be enabled/disabled."
     );
}

FileDownloader::FileDownloader(QUrl imageUrl, QStatusBar* statusBar, QObject *parent) : QObject(parent), reply_(nullptr), statusBar_(statusBar)
{
 	connect(&webCtrl_, SIGNAL(finished(QNetworkReply*)), this, SLOT(fileDownloaded(QNetworkReply*)));

 	QNetworkRequest request(imageUrl);
	reply_ = webCtrl_.get(request);
  connect(reply_, SIGNAL(downloadProgress(qint64, qint64)), this, SLOT(downloadProgress(qint64, qint64)));
}

void FileDownloader::fileDownloaded(QNetworkReply* reply)
{
  downloadedData_ = reply->readAll();
	reply->deleteLater();
  Q_EMIT downloaded();
}

void FileDownloader::downloadProgress(qint64 received, qint64 total) const
{
  if (statusBar_)
	{
    auto totalStr = total == -1 ? "?" : QString::number(total);
    statusBar_->showMessage(tr("File progress: %1 / %2").arg(received).arg(totalStr), 1000);
		if (received == total)
			statusBar_->showMessage("File downloaded.", 1000);
	}
}

void SCIRunMainWindow::toolkitDownload()
{
  auto action = qobject_cast<QAction*>(sender());

	static std::vector<ToolkitDownloader*> downloaders;
  downloaders.push_back(new ToolkitDownloader(action, statusBar(), this));
}

ToolkitDownloader::ToolkitDownloader(QObject* infoObject, QStatusBar* statusBar, QWidget* parent) : QObject(parent), iconDownloader_(nullptr), zipDownloader_(nullptr), statusBar_(statusBar)
{
  if (infoObject)
  {
    iconUrl_ = infoObject->property(ToolkitInfo::ToolkitIconURL).toString();
    fileUrl_ = infoObject->property(ToolkitInfo::ToolkitURL).toString();
    filename_ = infoObject->property(ToolkitInfo::ToolkitFilename).toString();

    iconKey_ = ToolkitInfo::ToolkitIconURL + QString("--") + iconUrl_;

    downloadIcon();
  }
}

void ToolkitDownloader::downloadIcon()
{
  QSettings settings;
  if (!settings.contains(iconKey_))
  {
    iconDownloader_ = new FileDownloader(iconUrl_, nullptr, this);
    connect(iconDownloader_, SIGNAL(downloaded()), this, SLOT(showMessageBox()));
  }
  else
    showMessageBox();
}

void ToolkitDownloader::showMessageBox()
{
  QPixmap image;

  QSettings settings;

  if (settings.contains(iconKey_))
  {
    image.loadFromData(settings.value(iconKey_).toByteArray());
  }
  else
  {
    if (!iconDownloader_)
      return;

    image.loadFromData(iconDownloader_->downloadedData());
    settings.setValue(iconKey_, iconDownloader_->downloadedData());
  }

  QMessageBox toolkitInfo;
#ifdef WIN32
  toolkitInfo.setWindowTitle("Toolkit information");
#else
  toolkitInfo.setText("Toolkit information");
#endif
  toolkitInfo.setInformativeText("Click OK to download the latest version of this toolkit:\n\n" + fileUrl_);
  toolkitInfo.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
  toolkitInfo.setIconPixmap(image);
  toolkitInfo.setDefaultButton(QMessageBox::Ok);
  toolkitInfo.show();
  auto choice = toolkitInfo.exec();

  if (choice == QMessageBox::Ok)
  {
    auto dir = QFileDialog::getExistingDirectory(qobject_cast<QWidget*>(parent()), "Select toolkit directory", ".");
    if (!dir.isEmpty())
    {
      toolkitDir_ = dir;
      zipDownloader_ = new FileDownloader(fileUrl_, statusBar_, this);
      connect(zipDownloader_, SIGNAL(downloaded()), this, SLOT(saveToolkit()));
    }
  }
}

void ToolkitDownloader::saveToolkit() const
{
  if (!zipDownloader_)
    return;

  auto fullFilename = toolkitDir_.filePath(filename_);
  QFile file(fullFilename);
  file.open(QIODevice::WriteOnly);
  file.write(zipDownloader_->downloadedData());
  file.close();
	statusBar_->showMessage("Toolkit file saved.", 1000);
}

void SCIRunMainWindow::loadToolkit()
{
  auto filename = QFileDialog::getOpenFileName(this, "Load Toolkit...", latestNetworkDirectory_.path(), "*.toolkit");
  loadToolkitsFromFile(filename);
}

void SCIRunMainWindow::loadToolkitsFromFile(const QString& filename)
{
  if (!filename.isEmpty())
  {
    ToolkitUnpackerCommand command;
    command.set(Variables::Filename, filename.toStdString());

    {
      auto file = command.get(Variables::Filename).toFilename();
      auto stem = QString::fromStdString(file.leaf().stem().string());
      auto dir = QString::fromStdString(file.parent_path().string());
      auto added = toolkitDirectories_.contains(stem);
      if (added && toolkitDirectories_[stem] == dir)
      {
        auto replace = QMessageBox::warning(this, "Toolkit already loaded",
          "Toolkit " + filename + " is already loaded. Do you wish to overwrite?", QMessageBox::Yes | QMessageBox::No);

        if (QMessageBox::No == replace)
          return;
        else
        {
          auto path = QString::fromStdString(file.string());
          auto toRemove = toolkitMenus_[path];
          if (toRemove)
          {
            menuToolkits_->removeAction(toolkitMenus_[path]->menuAction());
            toolkitMenus_.remove(path);
            toolkitFiles_.removeAll(filename);
            importedToolkits_.removeAll(filename);
          }
          else
            qDebug() << "logical error in toolkit removal";
        }
      }
    }

    if (command.execute())
    {
      statusBar()->showMessage(tr("Toolkit imported: ") + filename, 2000);
      if (!toolkitFiles_.contains(filename))
      {
        toolkitFiles_ << filename;
      }
      importedToolkits_ << filename;
    }
    else
    {
      statusBar()->showMessage(tr("Toolkit import failed: ") + filename, 2000);
      toolkitFiles_.removeAll(filename);
    }
  }
}

void SCIRunMainWindow::addToolkit(const QString& filename, const QString& directory, const ToolkitFile& toolkit)
{
  auto menu = menuToolkits_->addMenu(filename);
  auto networks = menu->addMenu("Networks");
  toolkitDirectories_[filename] = directory;
  toolkitNetworks_[filename] = toolkit;
  auto fullpath = directory + QDir::separator() + filename + ".toolkit";
  toolkitMenus_[fullpath] = menu;
  std::map<std::string, std::map<std::string, NetworkFile>> toolkitMenuData;
  for (const auto& toolkitPair : toolkit.networks)
  {
    std::vector<std::string> elements;
    for (const auto& p : boost::filesystem::path(toolkitPair.first))
      elements.emplace_back(p.filename().string());

    if (elements.size() == 2)
    {
      toolkitMenuData[elements[0]][elements[1]] = toolkitPair.second;
    }
    else
    {
      qDebug() << "Cannot handle toolkit folders of depth > 1";
    }
  }

  for (const auto& t1 : toolkitMenuData)
  {
    auto t1menu = networks->addMenu(QString::fromStdString(t1.first));
    for (const auto& t2 : t1.second)
    {
      auto networkAction = t1menu->addAction(QString::fromStdString(t2.first));
      std::ostringstream net;
      XMLSerializer::save_xml(t2.second, net, "networkFile");
      networkAction->setProperty("network", QString::fromStdString(net.str()));
      connect(networkAction, SIGNAL(triggered()), this, SLOT(openToolkitNetwork()));
    }
  }

  auto folder = menu->addAction("Open Toolkit Directory");
  folder->setProperty("path", directory);
  connect(folder, SIGNAL(triggered()), this, SLOT(openToolkitFolder()));

  auto remove = menu->addAction("Remove Toolkit...");
  remove->setProperty("filename", filename);
  remove->setProperty("fullpath", fullpath);
  connect(remove, SIGNAL(triggered()), this, SLOT(removeToolkit()));

  if (!startup_)
  {
    QMessageBox::information(this, "Toolkit loaded", "Toolkit " + filename +
      " successfully imported. A new submenu is available under Toolkits for loading networks.\n\n"
      + "Remember to update your data folder under Preferences->Paths.");
    actionPreferences_->trigger();
  }
}

void SCIRunMainWindow::openToolkitFolder()
{
  auto path = sender()->property("path").toString();
  QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

void SCIRunMainWindow::removeToolkit()
{
  auto filename = sender()->property("filename").toString();
  int r = QMessageBox::warning(this, tr("SCIRun 5"), "Remove toolkit " + filename + "?", QMessageBox::Yes | QMessageBox::No);
  if (QMessageBox::Yes == r)
  {
    menuToolkits_->removeAction(qobject_cast<QMenu*>(sender()->parent())->menuAction());
    auto fullpath = sender()->property("fullpath").toString();
    toolkitFiles_.removeAll(fullpath);
    importedToolkits_.removeAll(fullpath);
  }
}

void SCIRunMainWindow::openToolkitNetwork()
{
  if (okToContinue())
  {
    QTemporaryFile temp;
    if (temp.open())
    {
      auto network = sender()->property("network").toString();
      QTextStream stream(&temp);
      stream << network;
    }
    loadNetworkFile(temp.fileName(), true);
  }
}

void SCIRunMainWindow::launchNewInstance()
{
  #ifdef __APPLE__
  //TODO: test with bundle/installer
  auto appFilepath = Core::Application::Instance().executablePath();
  qDebug() << Core::Application::Instance().applicationName().c_str();
  qDebug() << appFilepath.string().c_str();
  qDebug() << appFilepath.parent_path().parent_path().string().c_str();
  auto command = "open -n " +
    (appFilepath.parent_path().parent_path() / "SCIRun/SCIRun_test").string() + " &";
  qDebug() << command.c_str();
  system( command.c_str() );

  #endif
}
