import CopyButton from './CopyButton.jsx'
import { formatBuildReport } from '../lib/format.js'
import { t } from '../i18n/index.js'

/**
 * What the core read out of BlackOps3.exe's header, as `InstallState.game_build`.
 *
 * Three outcomes, and two of them are silence:
 *   - null      - no exe, or a header the bounds-checked parser would not read. The folder rows
 *                 already say "no BlackOps3.exe here"; a second sentence about it is noise.
 *   - supported - a quiet one-liner, and only where the caller asks for it (`quiet`). Nothing
 *                 about a build the patch already knows deserves the user's attention.
 *   - not       - a warning with the three values, because the next thing the user should do is
 *                 report them, and copying beats retyping hex. (Retail exes keep 0x200
 *                 alignment, so a SizeOfImage like 0x1D75BC00 is real, not a typo.)
 *
 * A warning and never a gate. The on-disk header equalling the one the DLL reads in memory is
 * unverified for this Arxan-packed exe, and the DLL makes the real decision anyway - so blocking
 * install here would be this tool overruling the patch on a guess.
 */
export default function BuildNotice({ build, quiet = false, onCopy }) {
  if (!build) return null
  if (build.supported) {
    return quiet ? <p className="wiz-note build-ok">{t('build.supported')}</p> : null
  }
  const report = formatBuildReport(build)
  return (
    <div className="build-warn">
      <p className="build-warn-title">{t('build.unsupportedTitle')}</p>
      <p className="build-warn-body">{t('build.unsupportedBody')}</p>
      <div className="build-warn-report">
        <pre className="build-report">{report}</pre>
        <CopyButton
          size="sm"
          value={report}
          onCopy={onCopy}
          labels={{ copy: t('action.copy'), copied: t('action.copied'), failed: t('action.copyFailed') }}
        />
      </div>
    </div>
  )
}
