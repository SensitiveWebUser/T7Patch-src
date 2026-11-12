// The English catalogue, and the fallback every other locale falls back to.
//
// Flat dot-namespaced keys, not nested objects: lookup is a single property read, a missing key is
// one `undefined` rather than a walk that can fault halfway, and the whole file greps as
// `'area.name'` so a key in JSX can be found here by pasting it.
//
// Every value is a template with {0}-positional holes. Deliberately NOT named holes: this app has
// no plurals, no genders and no dates to format, and positional holes keep `t()` at one regex.
//
// Three sources were merged into this file so nothing was lost in the move off .NET:
//   1. every literal the old single-file App.jsx rendered or logged;
//   2. LeaveJustInCase/injector/T7Patch.Injector.UI/Resources/Strings.resx plus its Core sibling -
//      the predecessor's full inventory, which is a superset and carries error prose this app
//      never had;
//   3. the error sentinels in the Tauri command surface. Rust returns NEEDS_ELEVATION and friends
//      as bare tokens now, so the only user-facing wording for them lives here.
//
// Tombstone: Strings.resx's Startup_MissingGtk and Startup_GenericFailure_Format are deliberately
// absent. They fired before the Eto window existed; the Tauri equivalent fails before any webview
// is created and is reported by main.rs's panic hook into T7Patch-error.log. A JS string could
// never be read at that point, so carrying it here would only be a lie about coverage.
export default {
  // --- Application chrome -------------------------------------------------------------------
  'app.title': 'T7Patch',
  'app.version': 'T7Patch {0}',
  'tab.list': 'Sections',
  'tab.basic': 'Basic',
  'tab.advanced': 'Advanced',

  // --- Shared actions -----------------------------------------------------------------------
  'action.browse': 'Browse...',
  'action.copy': 'Copy',
  'action.copied': 'Copied',
  'action.generate': 'Generate',
  'action.show': 'Show',
  'action.hide': 'Hide',
  'action.finish': 'Finish',
  'action.skipSetup': 'Skip setup',
  'action.checkNow': 'Check now',
  'action.ok': 'OK',

  // --- Basic page ---------------------------------------------------------------------------
  'basic.play': 'Play',
  'basic.launchGame': 'Launch game',
  'basic.player': 'Player',
  'basic.playerName': 'Player name',
  'basic.friendsOnly': 'Friends only',
  'basic.friendsOnlyHint': 'Only people on your friends list can join your lobby.',
  'basic.autoInstallMods': 'Auto-install workshop mods',
  'basic.autoInstallModsHint': 'When a lobby needs a Steam Workshop mod you do not have, T7Patch installs it for you.',
  'basic.password': 'Password',
  'basic.passwordHint': 'Recommended. Filters casual join attempts, not real attackers.',

  // --- Advanced page ------------------------------------------------------------------------
  'advanced.gameFolder': 'Game folder',
  'advanced.gameFolderPlaceholder': '(not found automatically - browse for it)',
  'advanced.installPatch': 'Install T7Patch',
  'advanced.reinstall': 'Reinstall T7Patch',
  'advanced.reinstallHint': 'Reinstalls T7Patch using the files built into this copy of the tool.',
  'advanced.uninstall': 'Uninstall',
  'advanced.uninstallButton': 'Uninstall T7Patch',
  'advanced.uninstallHint': 'Removes dsound.dll, t7patch.dll, and t7patch.conf from the game folder, including your saved player name, password, and preferences.',
  'advanced.updates': 'Updates',
  'advanced.updatesHint': "Checks upstream Scroptss/T7Patch's GitHub releases - never replaces this tool automatically, only offers to open the download page.",
  'advanced.checkOnLaunch': 'Check for updates on launch',
  'advanced.autoLaunch': 'Auto-launch on startup',
  'advanced.steamLaunchOptions': 'Steam launch options',
  'advanced.steamLaunchOptionsHint': "Add this to the game's Steam launch options so Wine prefers the hijacked dsound.dll:",

  // --- What is actually on disk (install_state) ---------------------------------------------
  // These describe the folder rather than announcing an outcome, which is the whole point of
  // probing it: the UI can no longer disagree with reality about whether the patch is installed.
  'state.noFolder': 'No game folder chosen yet.',
  'state.notGameDir': 'No BlackOps3.exe here - this does not look like the game folder.',
  'state.installed': 'T7Patch is installed in this folder.',
  'state.notInstalled': 'T7Patch is not installed in this folder yet.',
  'state.partial': 'Half installed: {0} is here but {1} is not, so the patch will not load. Reinstall to fix it.',

  // --- Game build (InstallState.game_build) ---------------------------------------------------
  // Read from BlackOps3.exe's header on disk. Whether that equals what the DLL sees in memory is
  // unverified for this Arxan-packed exe, so the wording says what the patch WILL do on a build
  // it does not know, and never forbids installing: the check is the DLL's, not this tool's.
  'build.unsupportedBadge': 'This game build is not supported yet.',
  'build.unsupportedTitle': 'T7Patch does not support this game build yet; the patch will not apply.',
  'build.unsupportedBody': 'Black Ops III has probably been updated. You can still install, but the game will start without T7Patch\'s protection - and without any message saying so - until a T7Patch release supports this build. If you report it, include these values:',
  'build.supported': 'This game build is supported.',

  // --- Onboarding strip ---------------------------------------------------------------------
  'onboard.locateNotFound': "Couldn't find it automatically - browse for the folder containing BlackOps3.exe.",
  'onboard.installReady': 'Click Next to write your settings and install T7Patch.',
  'onboard.doneBody': 'T7Patch is installed and your settings have been saved. Launch the game from Steam as usual - the patch loads with it.',

  // --- Install ------------------------------------------------------------------------------
  'install.succeeded': 'Installed dsound.dll and t7patch.dll into {0}.',
  'install.exportsVerified': 'Installed. {0} exports verified.',
  'install.displacedDsound': 'There was already a dsound.dll here that was not ours, so it was renamed to {0} rather than overwritten. Uninstalling puts it back.',
  'install.noFolder': 'No game folder selected - browse for it first.',
  'install.failed': 'Install failed: {0}',

  // --- Uninstall ----------------------------------------------------------------------------
  'uninstall.confirmTitle': 'Uninstall T7Patch?',
  'uninstall.confirmBody': 'This removes dsound.dll, t7patch.dll, and t7patch.conf from {0}. Your player name, password, and other saved settings live in that file and will be deleted with it. This cannot be undone. Continue?',
  'uninstall.succeeded': 'Removed T7Patch from {0}.',
  'uninstall.failed': 'Uninstall failed: {0}',

  // --- Settings file --------------------------------------------------------------------------
  'config.saveFailed': 'Save failed: {0}',
  'config.loadFailed': "Couldn't load existing settings, using defaults: {0}",

  // --- Update check (fetch() straight to the GitHub API, no Rust command) --------------------
  'update.availableTitle': 'Update available',
  'update.upToDate': "No new release since {0} - you're already looking at the latest.",
  'update.failed': "Couldn't finish checking for updates: {0}",

  // --- Live injection -------------------------------------------------------------------------
  // The nine outcomes the predecessor's WindowsInjector distinguished. Keeping them apart is the
  // point: CannotOpenProcess means "the game is elevated and this tool is not" and calls for the
  // elevation path, LoadFailed means the DLL is bad and calls for a reinstall. One bare "failed"
  // tells the user which to try only by accident.
  // The three-state protection badge. Derived from what patch/Protection.cpp and
  // dsound-proxy actually require on disk - see the model note in App.jsx.
// The three states. One badge, three words - the whole point is that they are read at a glance.
  'protection.none': 'Not protected',
  'protection.partial': 'Partially protected',
  'protection.full': 'Fully protected',
  // The badge is a status region; this is what it announces, with {0} one of the three above.
  'protection.aria': 'Protection status: {0}',

  // One clause per row of the protection model: what is true about this folder.
  'protection.fullDetail': 'T7Patch loads with the game.',
  'protection.wrongFolder': 'Installed here, but there is no BlackOps3.exe in this folder.',
  'protection.foreignBlocked': 'Another dsound.dll is loading instead of ours - the patch will not load.',
  // {0} is a literal filename - "dsound.dll" or "t7patch.dll" - and is not translated.
  'protection.halfInstalled': '{0} is missing, so the patch will not load.',

  // ...and what to do about it. Separate from the clause above because the clause is a fact and
  // this is an instruction; a translator should be able to reorder them.
  'protection.actionBrowse': 'Choose the folder containing BlackOps3.exe.',
  'protection.actionInstall': 'Install T7Patch.',
  'protection.actionReinstall': 'Reinstall T7Patch.',
  'protection.actionMoveAside': 'Reinstall to move it aside; uninstalling puts it back.',

// --- The Setup tab ------------------------------------------------------------------------
  // A tab, not a dialog: it exists only while install_state says something is outstanding (or
  // while the user is in it), and it is the leftmost tab for as long as it exists.
  'tab.setup': 'Setup',
  // On the badge's row, and only when setup has been dismissed while something is still wrong.
  'wizard.reopen': 'Set up',

  'wizard.intro': 'Three things have to be true before the patch loads with the game. Anything already done is ticked - leave it alone.',
  'wizard.introDone': "Everything is in place. Nothing here needs doing again - it's kept so you can check or change it.",
  // Linux only. The three probed steps cannot cover the Steam launch option, so neither sentence
  // may count "three things" or say "everything" there.
  'wizard.introLaunchOption': 'Three things have to be true in the game folder, and on Linux the game also needs a Steam launch option. Anything already done is ticked - leave it alone.',
  'wizard.introDoneUnsupported': 'The game folder is set up, but this game build is not supported yet - see below.',
  'wizard.introDoneLaunchOption': 'The game folder is set up. The last step is the Steam launch option, which this tool cannot check for you.',

  'wizard.stepFolder': 'Point at the game folder',
  'wizard.stepSettings': 'Save your settings',
  'wizard.stepInstall': 'Install the patch',
  'wizard.stepPlay': 'Ready to play',

  // Announced with the step's name; the tick and the empty ring are what a sighted user sees.
  'wizard.stepDone': 'Done',
  'wizard.stepTodo': 'Still to do',

  'wizard.folderFound': 'BlackOps3.exe is in this folder.',
  'wizard.settingsNew': 'These are written to t7patch.conf in the game folder.',
  'wizard.settingsSaved': 't7patch.conf has been written. Changes here save themselves as you type.',
  'wizard.saveSettings': 'Save settings',
  'wizard.installDone': 'dsound.dll and t7patch.dll are both in place.',
  'wizard.installTodo': 'Copies dsound.dll and t7patch.dll into the game folder. Nothing outside that folder is touched.',
  'wizard.playBlocked': 'Finish the steps above and this becomes available.',
  'wizard.playNeedsLaunchOption': 'The files are in place. On Linux the patch still needs the Steam launch option below.',

  // --- Basic page ---------------------------------------------------------------------------
  // Moved off Advanced. The wording is new because "Auto-launch on startup" never said whose
  // startup it meant, and it now sits directly above the button it is about.
  'basic.autoLaunch': 'Launch the game when this tool opens',
  'basic.autoLaunchHint': 'Starts Black Ops III through Steam as soon as this window appears.',

  // --- Advanced page ------------------------------------------------------------------------
  'advanced.runSetup': 'Setup',
  'advanced.runSetupHint': 'Reopens the guided setup. It re-checks the folder, so it will not offer you anything that is already done.',
  'advanced.runSetupButton': 'Run setup',

  // STAGED, not dead: live injection has a working Rust backend and no UI yet
  // (PARITY-TODO.md). These are the predecessor's wording, kept so the feature does not have
  // to reinvent it. Delete them only if the feature is abandoned.
  'inject.selectDll': 'Select t7patch.dll to inject',
  'inject.noProcess': 'BlackOps3.exe is not running.',
  'inject.chooseProcess': 'More than one Black Ops III is running - pick the one to inject into.',
  'inject.processLabel': 'pid {0} - {1}',
  'inject.injecting': 'Injecting...',
  'inject.succeeded': 'Injected {0} into BlackOps3.exe (pid {1}).',
  'inject.alreadyLoaded': 'A t7patch.dll is already loaded in the game - leaving it alone. Whatever loaded it (the dsound.dll hijack, or an earlier injection) owns activating it.',
  'inject.targetInvalid': "The running process named BlackOps3 doesn't look like a real Black Ops III install - refusing to inject into it.",
  'inject.targetUnreadable': "Couldn't verify the running BlackOps3 process before injecting: {0}",
  'inject.failed': 'Injection failed: {0}',
  'inject.failedWithCode': 'Injection failed: {0} (Win32 error {1}).',
  'inject.windowsOnly': 'Live injection only works on Windows.',
  'inject.reasonCannotOpenProcess': "couldn't open the game process - it may be running elevated while this tool isn't",
  'inject.reasonRemoteMemoryFailed': "couldn't allocate or write memory in the game process",
  'inject.reasonCannotStartRemoteThread': "couldn't start a thread in the game process",
  'inject.reasonLoadTimedOut': "the game didn't finish loading the DLL in time",
  'inject.reasonLoadFailed': 'the game refused to load the DLL',
  'inject.reasonModuleNotFound': "couldn't read the game's module list, so the DLL can't be located or activated",
  'inject.reasonActivationExportsMissing': "that file loaded but isn't a t7patch.dll - it has no activation exports",
  'inject.reasonUnexpected': 'an unexpected error',

  // --- Error sentinels ------------------------------------------------------------------------
  // Rust returns these as bare tokens, compared EXACTLY on the JS side. The prose lives here and
  // only here, which is what makes them translatable at all - and what stops a reworded message
  // from silently breaking a substring match, the way it broke elevation once already.
  'error.gameIsRunning': 'Black Ops III is running and has the files open. Close the game, then install.',
  'error.needsElevation': 'That folder needs administrator rights to write to - this is common when the game is installed under Program Files.',
  'error.embeddedPayloadInvalid': 'This copy of the tool has a damaged built-in t7patch.dll: it is missing {0}. Nothing was written to the game folder. Download the tool again.',
  'error.elevateFailed': 'Could not restart with administrator rights.',

  // --- File and folder pickers ----------------------------------------------------------------
  'file.selectGameFolder': 'Select the folder containing BlackOps3.exe',

  // --- Status log -----------------------------------------------------------------------------
  // Restored: the log had a Clear button in the predecessor, the rewrite dropped it, and
  // then a prune-unreferenced-strings pass deleted this key BECAUSE nothing referenced it -
  // erasing the only remaining evidence of the regression. Reference count is not deadness.
  'status.clear': 'Clear',
  'status.title': 'Status',
  'status.empty': 'Nothing has happened yet.',
  // 'status.line' was here. Removed deliberately: the bracketed timestamp is a wire format
  // between format.js's formatLogLine and StatusLog's parser, not user prose. Translating it
  // (moving the bracket, using a full-width one) silently breaks every line's timestamp column.
  'log.foundGameFolder': 'Found game folder: {0}',
  'log.gameFolderNotFound': 'Game folder not found automatically - browse for it on the Advanced page.',
  'log.locateFailed': 'Could not look for the game folder: {0}',
  'log.gameFolderSet': 'Game folder set: {0}',
  'log.launching': 'Launching the game via Steam...',
  'log.launchFailed': "Couldn't launch the game: {0}. Is Steam installed?",
  'log.passwordCopied': 'Password copied.',
  'log.settingsSaved': 'Settings saved.',
  'log.buildReportCopied': 'Game build details copied.',

  // The foreign-dsound case. New in this implementation - the predecessor overwrote a
  // pre-existing dsound.dll without noticing, which is the defect this wording exists to report.
  'state.foreignDsound': 'Something else is using dsound.dll - DSOAL, dgVoodoo or similar. Installing moves it aside to dsound.dll.before-t7patch and uninstalling puts it back, so nothing of yours is lost.',
  'warn.installedIntoNonGameDir': 'Installed, but there is no BlackOps3.exe in that folder - the game will not load these files from there.',
  'log.stateFailed': "Couldn't check the game folder: {0}",
  'action.copyFailed': 'Copy failed',
}
