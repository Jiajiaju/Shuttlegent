/*
 * Panel text through i18next. The editor opens the page with ?lang=<locale> taken from the editor's display language
 * (SShuttlegentWebPanel::GetPageUrl) and reloads the page when that language changes, so the language is fixed for the
 * lifetime of a page and module-level constants may call t(). Strings live in i18n/<locale>.js, which fill
 * window.ShuttlegentLocales before this script runs; English is the source language and the fallback.
 *
 * Static markup is translated by attribute: data-i18n (text), data-i18n-title, data-i18n-aria-label and
 * data-i18n-placeholder each name a key. Build/Web/check-i18n.mjs checks keys, usage and leftover Chinese literals.
 */
(function () {
  'use strict';

  const SUPPORTED = ['en', 'zh-Hans'];
  const FALLBACK = 'en';

  const requested = new URLSearchParams(window.location.search).get('lang');
  const locale = SUPPORTED.includes(requested) ? requested : FALLBACK;

  const tables = window.ShuttlegentLocales || {};
  const resources = {};
  for (const language of SUPPORTED) {
    if (tables[language]) resources[language] = { translation: tables[language] };
  }

  const i18n = window.i18next.createInstance();

  // Values are escaped by the caller where the result goes into HTML, so i18next must not escape them again
  i18n.init({ lng: locale, fallbackLng: FALLBACK, supportedLngs: SUPPORTED, resources: resources, initAsync: false, returnNull: false, interpolation: { escapeValue: false } });
  document.documentElement.lang = locale;

  const t = (key, options) => i18n.t(key, options);

  const ATTRIBUTES = [['data-i18n-title', 'title'], ['data-i18n-aria-label', 'aria-label'], ['data-i18n-placeholder', 'placeholder']];

  /** Translates the static markup under root: text of [data-i18n] elements and the attributes named above. */
  function applyStatic(root) {
    for (const element of root.querySelectorAll('[data-i18n]')) element.textContent = t(element.getAttribute('data-i18n'));
    for (const [source, target] of ATTRIBUTES) {
      for (const element of root.querySelectorAll('[' + source + ']')) element.setAttribute(target, t(element.getAttribute(source)));
    }
  }

  const numberFormat = new Intl.NumberFormat(locale);
  const monthDayFormat = new Intl.DateTimeFormat(locale, { month: 'long', day: 'numeric' });

  applyStatic(document);

  window.ShuttlegentI18n = {
    locale: locale,
    t: t,
    applyStatic: applyStatic,
    formatNumber: (value) => numberFormat.format(value),
    formatMonthDay: (time) => monthDayFormat.format(new Date(time))
  };
})();
