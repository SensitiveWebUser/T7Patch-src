# i18n

Adding a language, in five steps:

1. Copy `en.js` to `<tag>.js` (a BCP 47 tag, lowercase language + uppercase region: `de.js`, `pt-BR.js`) and translate the values, leaving every key and every `{0}` hole exactly as it is.
2. Import it in `index.js` and add it to the `CATALOGUES` map — that map is the whole registry, and its keys are what `navigator.language` is matched against.
3. Delete any line you have not translated rather than leaving it in English: `t()` falls back per key, so a missing key shows English while a copied one claims to be translated.
4. Run the app with the browser or desktop locale set to that tag; `resolve()` narrows `pt-BR` → `pt` → `en`, so a regional catalogue and a plain-language one can both exist.
5. Check the dev console for `[i18n] missing key:` — anything logged there is a string still hardcoded in a component, and belongs in `en.js` first.
