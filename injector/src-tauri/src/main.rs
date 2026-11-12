#![cfg_attr(not(debug_assertions), windows_subsystem = "windows")]

// Tauri chooses between the embedded frontend and `devUrl` on this cargo feature and NOTHING
// else - not the profile, not debug_assertions. tauri-macros does
// `dev: cfg!(not(feature = "custom-protocol"))`, and tauri-codegen then takes the
// `dev && dev_url.is_some()` branch, which embeds `EmbeddedAssets::default()` - an EMPTY asset
// set - and points the window at http://localhost:5173.
//
// The `tauri` CLI passes `--features custom-protocol` on `tauri build`. A plain `cargo build`
// does not, which is exactly how a release binary shipped that opened a window, failed with
// "Could not connect to localhost: Connection refused", and exited. Nothing about that failure
// pointed at a missing cargo feature.
//
// So this is a hard build error rather than a comment. A release build without the feature is
// never what anyone wants, and the runtime symptom is too far from the cause to debug twice.
#[cfg(all(not(debug_assertions), not(feature = "custom-protocol")))]
compile_error!(
    "a release build needs the `custom-protocol` feature, or Tauri embeds no frontend at all and \
     the binary tries to reach a dev server that is not running. It is on by default; you have \
     disabled default features."
);

use std::path::PathBuf;
use t7patch_core as core;

#[tauri::command]
fn locate_game() -> Option<String> {
    core::find_game().map(|p| p.to_string_lossy().into_owned())
}

#[tauri::command]
fn load_config(dir: String) -> core::PatchConfig {
    core::PatchConfig::load(&PathBuf::from(dir))
}

#[tauri::command]
fn save_config(dir: String, cfg: core::PatchConfig) -> Result<(), String> {
    cfg.save(&PathBuf::from(dir)).map_err(|e| e.to_string())
}

/// Installs, returning a structured report rather than prose. The frontend owns the wording,
/// which is also what makes these strings translatable at all - see REFACTOR-PLAN.md.
#[tauri::command]
fn install(dir: String) -> Result<core::InstallReport, String> {
    let d = PathBuf::from(dir);

    // Verified BEFORE anything is written. The old order installed both DLLs, then verified, then
    // returned Err on failure - leaving a fully armed install on disk while telling the user it
    // had failed, so their next launch loaded a DLL the tool had just disowned. The payload is a
    // compile-time constant, so this cannot fail for a reason the user caused, but it is exactly
    // the check that catches a corrupt or wrong-arch embedded blob at the only point where
    // refusing still leaves the folder untouched.
    //
    // It also checks the RIGHT exports. It used to look for SetPlayerName, which the loader never
    // calls; the two that matter are the activation pair, because without them a loaded DLL sits
    // inert - see patch/dllmain.cpp.
    let staged = core::pe_exports_from_bytes(core::T7PATCH).map_err(|e| e.to_string())?;
    for needed in ["EnableInjectorlessInstall", "zbr_run_gamemode_lui"] {
        if !staged.iter().any(|e| e == needed) {
            return Err(format!("EMBEDDED_PAYLOAD_INVALID:{needed}"));
        }
    }

    match core::install_hijack(&d) {
        Ok(mut report) => {
            report.exports_verified = staged.len();
            Ok(report)
        }
        Err(e) if core::is_file_in_use(&e) => Err("GAME_IS_RUNNING".into()),
        Err(e) if core::is_permission_denied(&e) => Err("NEEDS_ELEVATION".into()),
        Err(e) => Err(e.to_string()),
    }
}

#[tauri::command]
fn uninstall(dir: String) -> Result<(), String> {
    core::uninstall_hijack(&PathBuf::from(dir)).map_err(|e| e.to_string())
}

/// The version this build reports. The update CHECK itself is done by the frontend with fetch()
/// - see the tombstone in core - but the version it compares against belongs here, so there is
/// one source of truth for what this binary is rather than a literal duplicated in JS.
#[tauri::command]
fn current_version() -> &'static str {
    core::CURRENT_VERSION
}

/// What is actually on disk in the game folder, so the UI can reflect reality instead of
/// declaring outcomes. The wizard used to render "T7Patch is installed" as an unconditional
/// literal because it was a step counter with no idea what had happened; probing removes that
/// whole class of disagreement rather than fixing it case by case.
#[tauri::command]
fn install_state(dir: String) -> core::InstallState {
    core::install_state(&PathBuf::from(dir))
}

#[tauri::command]
fn launch_game() -> Result<(), String> {
    core::launch_game().map_err(|e| e.to_string())
}

/// The Steam launch option the Linux hijack needs, for the user to copy. Windows needs none, and
/// None is what tells the frontend not to show that row at all.
#[tauri::command]
fn linux_launch_option() -> Option<&'static str> {
    if cfg!(windows) {
        None
    } else {
        Some(core::LINUX_LAUNCH_OPTION)
    }
}

#[tauri::command]
fn elevate() -> bool {
    core::relaunch_elevated()
}

/// Injects and activates `dll_path` in the process `pid`.
///
/// dll_path is an absolute path, never a bare filename: LoadLibraryW resolves a bare name through
/// the TARGET's DLL search order, which would load whatever t7patch.dll the game happens to find
/// rather than the one the user chose.
/// Running Black Ops III processes to offer as injection targets. Empty off Windows, where the
/// whole feature does not exist - the UI hides the card rather than showing a dead control.
#[tauri::command]
fn list_game_processes() -> Vec<core::GameProcess> {
    #[cfg(windows)]
    {
        core::inject::list_game_processes()
            .into_iter()
            .map(|(pid, path)| core::GameProcess { pid, path })
            .collect()
    }
    #[cfg(not(windows))]
    {
        Vec::new()
    }
}

#[tauri::command]
fn inject_into(pid: u32, dll_path: String) -> Result<(), String> {
    #[cfg(windows)]
    {
        // None means the module list could not be read, which is NOT the same as "no copy is
        // loaded". Injecting anyway risks a second copy: that gets its own g_patched == false,
        // so RunPatching's re-entry guard misses the first copy and Protection::install() runs
        // twice, which dllmain.cpp warns is not clean. The same API supplies the module base
        // needed to activate anything, so a load could not be followed up either. Refuse.
        match core::inject::module_loaded(pid, "t7patch.dll") {
            Some(true) => return Err("a t7patch.dll is already loaded in that process".into()),
            None => return Err("could not read the target's module list".into()),
            Some(false) => {}
        }
        return core::inject::inject(pid, &dll_path);
    }
    #[cfg(not(windows))]
    {
        let _ = (pid, dll_path);
        Err("injection is Windows-only".into())
    }
}

/// Writes an unexpected failure where a user can find it, next to the executable, and also to
/// stderr. This exists because the release profile sets `panic = "abort"` and `strip = true` for
/// binary size, which between them turn any panic into a silent instant exit with no backtrace
/// and no message - the failure mode the .NET build was specifically written not to have. The
/// hook still runs under `panic = "abort"`; only unwinding is disabled.
fn report(what: &str, detail: &str) {
    let line = format!("T7Patch (tauri): {what}: {detail}\n");
    eprint!("{line}");
    if let Ok(exe) = std::env::current_exe() {
        if let Some(dir) = exe.parent() {
            let _ = std::fs::OpenOptions::new()
                .create(true)
                .append(true)
                .open(dir.join("T7Patch-error.log"))
                .map(|mut f| std::io::Write::write_all(&mut f, line.as_bytes()));
        }
    }
}

fn main() {
    std::panic::set_hook(Box::new(|info| {
        report("panicked", &info.to_string());
    }));

    // Hardening, NOT the fix for the "opens then closes" report - that was the missing
    // custom-protocol feature above, and this was a wrong guess at it. Kept because the failure
    // it does prevent is real and has the same silent shape: WebKitGTK's DMABUF renderer kills
    // its own web process on a lot of Linux desktops (proprietary NVIDIA drivers, VMs with
    // virtualised GPUs, several Wayland compositors), and when that process dies the window goes
    // with it and the app exits with nothing printed. Software compositing costs nothing
    // perceptible for a form with four inputs.
    //
    // Set only if the user has not, so it stays overridable for anyone debugging the GPU path.
    // This is a cost of the webview stack rather than of this app: the GTK build has no second
    // process to lose.
    #[cfg(target_os = "linux")]
    {
        for (key, value) in [
            ("WEBKIT_DISABLE_DMABUF_RENDERER", "1"),
            ("WEBKIT_DISABLE_COMPOSITING_MODE", "1"),
        ] {
            if std::env::var_os(key).is_none() {
                std::env::set_var(key, value);
            }
        }
    }

    let result = tauri::Builder::default()
        .plugin(tauri_plugin_dialog::init())
        .plugin(tauri_plugin_clipboard_manager::init())
        .plugin(tauri_plugin_process::init())
        .invoke_handler(tauri::generate_handler![
            locate_game,
            load_config,
            save_config,
            install,
            uninstall,
            current_version,
            install_state,
            launch_game,
            linux_launch_option,
            elevate,
            list_game_processes,
            inject_into
        ])
        .run(tauri::generate_context!());

    // Not .expect(): that prints through the panic hook above but reads as a crash. A webview
    // that cannot be created is a plain environment failure and deserves a plain sentence.
    if let Err(e) = result {
        report("could not start", &e.to_string());
        std::process::exit(1);
    }
}
