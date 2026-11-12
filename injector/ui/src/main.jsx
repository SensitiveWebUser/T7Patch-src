import React from 'react'
import { createRoot } from 'react-dom/client'
import App from './App.jsx'
import ErrorBoundary from './components/ErrorBoundary.jsx'
import './theme.css'

// A render throw used to leave a blank webview with nothing written anywhere - the exact failure
// the Rust side installs a panic hook to avoid (it writes T7Patch-error.log next to the exe,
// because panic=abort + strip would otherwise make a panic a silent exit). The frontend needs the
// same courtesy, and a boundary only catches renders, so the two global handlers below cover the
// rest: this app's failure surface is mostly promises, not render throws.
const report = (what, detail) => {
  // eslint-disable-next-line no-console
  console.error(`T7Patch (ui): ${what}:`, detail)
}
window.addEventListener('error', (e) => report('error', e.error ?? e.message))
window.addEventListener('unhandledrejection', (e) => report('unhandled rejection', e.reason))

createRoot(document.getElementById('root')).render(
  <React.StrictMode>
    <ErrorBoundary>
      <App />
    </ErrorBoundary>
  </React.StrictMode>,
)
