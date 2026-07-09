// ==UserScript==
// @name         Lovense Equinox Bridge
// @namespace    io.github.lovense-equinox-bridge
// @version      1.0.0
// @description  Intercepts Lovense tip commands and forwards them to the local Equinox bridge
// @author       bridge
// @match        *://*.chaturbate.com/*
// @match        *://*.stripchat.com/*
// @match        *://*.bongacams.com/*
// @match        *://*.cams.com/*
// @match        *://*.cam4.com/*
// @match        *://*.myfreecams.com/*
// @match        *://*.livejasmin.com/*
// @match        *://*.streamate.com/*
// @match        *://*.camsoda.com/*
// @match        *://*.xmodels.com/*
// @match        *://*/*
// @run-at       document-start
// @grant        none
// ==/UserScript==

(function() {
    'use strict';

    let BRIDGE_URL = 'http://127.0.0.1:20010/command';
    const LOG_PREFIX = '[LovenseBridge]';

    // ---- Settings helpers (read from Lovense settings) ----
    function levelForTip(amount, settings) {
        if (!settings || !settings.levels) {
            return Math.min(20, Math.max(1, Math.floor(amount / 10)));
        }
        const levels = Object.values(settings.levels).sort((a, b) => a.min - b.min);
        for (const lvl of levels) {
            if (amount >= lvl.min && amount <= (lvl.max || Infinity)) {
                return lvl.vLevel;
            }
        }
        const last = levels[levels.length - 1];
        return last ? last.vLevel : Math.min(20, Math.floor(amount / 10));
    }

    async function sendToBridge(action, value, timeSec) {
        try {
            await fetch(BRIDGE_URL, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({
                    command: 'Function',
                    action: `${action}:${value}`,
                    timeSec: timeSec || 3,
                    apiVer: 1
                })
            });
        } catch (e) {
            console.warn(LOG_PREFIX, 'Bridge unreachable, make sure the bridge is running:', e.message);
        }
    }

    async function sendStopToBridge() {
        try {
            await fetch(BRIDGE_URL, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ command: 'Function', action: 'Stop', timeSec: 0, apiVer: 1 })
            });
        } catch (e) {}
    }

    // ---- Intercept window.lovense (Cam Kit for Web) ----
    let realLovense = null;
    Object.defineProperty(window, 'lovense', {
        configurable: true,
        enumerable: true,
        get() {
            return realLovense;
        },
        set(val) {
            if (val && val.receiveTip) {
                const origReceiveTip = val.receiveTip.bind(val);
                const origGetSettings = val.getSettings ? val.getSettings.bind(val) : null;
                let cachedSettings = null;

                val.getSettings = async function() {
                    cachedSettings = await origGetSettings();
                    return cachedSettings;
                };

                val.receiveTip = async function(cname, amount) {
                    console.log(LOG_PREFIX, `lovense.receiveTip: tipper=${cname}, amount=${amount}`);

                    if (origGetSettings && !cachedSettings) {
                        try { cachedSettings = await origGetSettings(); } catch(e) {}
                    }

                    const level = levelForTip(amount, cachedSettings);
                    console.log(LOG_PREFIX, `Mapping tip $${amount} → Vibrate:${level}`);
                    await sendToBridge('Vibrate', level, 5);
                    await origReceiveTip(cname, amount);
                };

                val.stop = async function() {
                    await sendStopToBridge();
                };

                console.log(LOG_PREFIX, 'Intercepted window.lovense');
            }
            realLovense = val;
        }
    });

    // ---- Intercept window.CamExtension (Cam Extension for Chrome) ----
    let realCamExtension = null;
    Object.defineProperty(window, 'CamExtension', {
        configurable: true,
        enumerable: true,
        get() {
            return realCamExtension;
        },
        set(val) {
            if (typeof val === 'function') {
                const OrigCamExt = val;
                const instances = new WeakSet();

                const ProxyCamExt = function(websiteName, modelName) {
                    const instance = new OrigCamExt(websiteName, modelName);
                    instances.add(instance);
                    wrapCamExtInstance(instance);
                    return instance;
                };

                ProxyCamExt.prototype = OrigCamExt.prototype;
                Object.setPrototypeOf(ProxyCamExt, OrigCamExt);

                console.log(LOG_PREFIX, 'Intercepted CamExtension constructor');
                realCamExtension = ProxyCamExt;
            } else if (val && typeof val === 'object') {
                wrapCamExtInstance(val);
                console.log(LOG_PREFIX, 'Intercepted CamExtension instance');
                realCamExtension = val;
            } else {
                realCamExtension = val;
            }
        }
    });

    function wrapCamExtInstance(instance) {
        const methods = ['receiveTip', 'getSettings', 'getToyStatus', 'getCamVersion', 'stop'];
        const patched = new Set();

        methods.forEach(method => {
            if (patched.has(method)) return;
            const orig = instance[method];
            if (typeof orig !== 'function') return;
            patched.add(method);

            instance[method] = async function(...args) {
                if (method === 'receiveTip') {
                    const [amount, tipperName] = args;
                    let settings;
                    try {
                        settings = await instance.getSettings();
                    } catch(e) {}
                    const level = levelForTip(amount, settings);
                    console.log(LOG_PREFIX, `CamExtension.receiveTip: tipper=${tipperName}, amount=${amount} → level=${level}`);
                    await sendToBridge('Vibrate', level, 5);
                } else if (method === 'stop') {
                    await sendStopToBridge();
                }
                return orig.call(instance, ...args);
            };
        });
    }

    // ---- Also intercept postMessage-based Lovense commands ----
    const origPostMessage = window.postMessage;
    window.postMessage = function(message, targetOrigin, transfer) {
        if (message && typeof message === 'object') {
            if (message.type === 'LOVENSE_VIBRATE' || message.type === 'lovense_vibrate') {
                sendToBridge('Vibrate', message.level || 10, message.duration || 5);
            } else if (message.type === 'LOVENSE_STOP' || message.type === 'lovense_stop') {
                sendStopToBridge();
            }
        }
        return origPostMessage.call(this, message, targetOrigin, transfer);
    };

    // ---- Also watch for the lovense message listener API ----
    const origAddEventListener = window.addEventListener;
    window.addEventListener = function(type, listener, options) {
        if (type === 'message') {
            const wrapped = function(event) {
                try {
                    const data = typeof event.data === 'string' ? JSON.parse(event.data) : event.data;
                    if (data && data.type === 'lovenseTip') {
                        sendToBridge('Vibrate', data.level || 10, 5);
                    }
                } catch(e) {}
                return listener.call(this, event);
            };
            return origAddEventListener.call(this, type, wrapped, options);
        }
        return origAddEventListener.call(this, type, listener, options);
    };

    // ---- Expose control API on window for debugging ----
    window.__lovenseBridge = {
        sendVibrate: (level, time) => sendToBridge('Vibrate', level, time),
        stop: () => sendStopToBridge(),
        setBridgeUrl: (url) => { BRIDGE_URL = url; },
        test: () => sendToBridge('Vibrate', 10, 2).then(() => console.log(LOG_PREFIX, 'Test OK')),
    };

    // Auto-test bridge on load
    window.addEventListener('load', () => {
        fetch(BRIDGE_URL, {
            method: 'POST',
            headers: { 'Content-Type': 'application/json' },
            body: JSON.stringify({ command: 'GetToys' })
        })
        .then(r => r.json())
        .then(data => {
            if (data.code === 200) {
                console.log(LOG_PREFIX, 'Bridge connected:', JSON.stringify(data.data));
            }
        })
        .catch(() => console.warn(LOG_PREFIX, 'Bridge not running. Start with: python bridge.py'));
    });

    console.log(LOG_PREFIX, 'Interceptor installed');
})();
