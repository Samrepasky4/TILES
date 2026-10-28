import { BrowserWindow as e, app as t, ipcMain as n } from "electron";
import { dirname as r, join as i } from "node:path";
import { fileURLToPath as a } from "node:url";
import { usb as o } from "usb";
//#region src/main/usb.ts
var s = null, c = null, l = null, u = null, d = "", f = Promise.resolve();
async function p() {
	return (await o.getDevices()).map((e) => ({
		vendorId: e.vendorId,
		productId: e.productId,
		productName: e.productName ?? void 0,
		manufacturerName: e.manufacturerName ?? void 0,
		serialNumber: e.serialNumber ?? void 0
	}));
}
async function m(e, t) {
	await h();
	let n = await o.findDeviceByIds(e, t);
	if (!n) throw Error(`USB device ${y(e)}:${y(t)} not found`);
	await n.open();
	let r = n.configuration?.interfaces.find((e) => e.alternate.interfaceClass === 255 && e.alternate.interfaceName === "SENTIA TILES Control");
	if (!r) throw await n.close(), Error("SENTIA TILES Control interface not found");
	let i = r.alternate.endpoints.find((e) => e.direction === "out"), a = r.alternate.endpoints.find((e) => e.direction === "in");
	if (!i || !a) throw await n.close(), Error("TILES control endpoints not found");
	return await n.claimInterface(r.interfaceNumber), s = n, c = r.interfaceNumber, u = i.endpointNumber, l = a.endpointNumber, d = "", console.log("TILES ready:", {
		interface: c,
		outEndpoint: u,
		inEndpoint: l
	}), {
		connected: !0,
		vendorId: n.vendorId,
		productId: n.productId,
		productName: n.productName,
		manufacturerName: n.manufacturerName
	};
}
async function h() {
	if (s) try {
		c !== null && await s.releaseInterface(c), await s.close();
	} catch (e) {
		console.warn("USB disconnect error:", e);
	}
	return s = null, c = null, l = null, u = null, d = "", { connected: !1 };
}
async function g() {
	if (!s || l === null) throw Error("TILES is not connected");
	for (; !d.includes("\n");) {
		let e = await s.transferIn(l, 64);
		if (e.status !== "ok" || !e.data) throw Error(`USB read failed: ${e.status}`);
		let t = new Uint8Array(e.data.buffer, e.data.byteOffset, e.data.byteLength);
		d += new TextDecoder().decode(t);
	}
	let e = d.indexOf("\n"), t = d.slice(0, e).replace(/\r$/, "");
	return d = d.slice(e + 1), t;
}
function _(e, t = !1) {
	let n = f.then(() => v(e, t));
	return f = n.then(() => void 0, () => void 0), n;
}
async function v(e, t = !1) {
	if (!s || u === null) throw Error("TILES is not connected");
	console.log(`→ TILES: ${e}`);
	let n = await s.transferOut(u, new TextEncoder().encode(`${e}\n`));
	if (n.status !== "ok") throw Error(`USB write failed: ${n.status}`);
	let r = [];
	for (;;) {
		let e = await g();
		if (console.log(`← TILES: ${e}`), t && e === "OK" || (r.push(e), !t || e.startsWith("ERR "))) return r;
	}
}
function y(e) {
	return `0x${e.toString(16).padStart(4, "0")}`;
}
var b = {
	_comment: "Captured from a real SENTIA TILES (board 2, firmware 0.2.0) with tools/tiles_control.py schema + list on 2026-09-29. For the companion app's fake device -- see ../BRIEF.md. 'value' is that board's current value; the real app must read it from the device.",
	firmware: "0.2.0",
	settings: [
		{
			id: 256,
			key: "pedal.mode",
			type: "enum",
			values: ["sustain", "expression"],
			default: "sustain",
			value: "sustain",
			persist: !0
		},
		{
			id: 257,
			key: "pedal.polarity",
			type: "enum",
			values: ["normally_open", "normally_closed"],
			default: "normally_open",
			value: "normally_open",
			persist: !0
		},
		{
			id: 258,
			key: "pedal.sustain_style",
			type: "enum",
			values: ["synth", "hold"],
			default: "synth",
			value: "synth",
			persist: !0
		},
		{
			id: 512,
			key: "expression.mpe_enabled",
			type: "bool",
			default: !0,
			value: !0,
			persist: !0
		},
		{
			id: 513,
			key: "expression.pitch_bend_sensitivity",
			type: "float",
			min: .001,
			max: 1,
			default: .065,
			value: .1,
			persist: !0
		},
		{
			id: 514,
			key: "expression.aftertouch_sensitivity",
			type: "uint",
			min: 1,
			max: 65535,
			default: 900,
			value: 1300,
			persist: !0
		},
		{
			id: 768,
			key: "cv_gate.enabled",
			type: "bool",
			default: !1,
			value: !1,
			persist: !1
		},
		{
			id: 769,
			key: "cv_gate.pitch.volts_per_semitone",
			type: "float",
			min: .001,
			max: 1,
			default: .083333,
			value: .083333,
			persist: !0
		},
		{
			id: 770,
			key: "cv_gate.pitch.reference_note",
			type: "int",
			min: 0,
			max: 127,
			default: 0,
			value: 0,
			persist: !0
		},
		{
			id: 771,
			key: "cv_gate.pitch.zero_trim_volts",
			type: "float",
			min: -2.5,
			max: 2.5,
			default: 0,
			value: 0,
			persist: !0
		},
		{
			id: 772,
			key: "cv_gate.pitch.gain_trim",
			type: "float",
			min: .5,
			max: 2,
			default: 1,
			value: 1,
			persist: !0
		},
		{
			id: 773,
			key: "cv_gate.pressure.full_scale_volts",
			type: "float",
			min: .1,
			max: 10,
			default: 10,
			value: 10,
			persist: !0
		},
		{
			id: 774,
			key: "cv_gate.pressure.zero_trim_volts",
			type: "float",
			min: -2.5,
			max: 2.5,
			default: 0,
			value: 0,
			persist: !0
		},
		{
			id: 775,
			key: "cv_gate.pressure.gain_trim",
			type: "float",
			min: .5,
			max: 2,
			default: 1,
			value: 1,
			persist: !0
		},
		{
			id: 1024,
			key: "look.idle_baseline_percent",
			type: "uint",
			min: 0,
			max: 100,
			default: 50,
			value: 50,
			persist: !0
		},
		{
			id: 1025,
			key: "look.natural_pad_percent",
			type: "uint",
			min: 0,
			max: 100,
			default: 21,
			value: 21,
			persist: !0
		},
		{
			id: 1026,
			key: "look.root_pad_percent",
			type: "uint",
			min: 0,
			max: 100,
			default: 40,
			value: 40,
			persist: !0
		},
		{
			id: 1027,
			key: "look.fifth_pad_percent",
			type: "uint",
			min: 0,
			max: 100,
			default: 40,
			value: 40,
			persist: !0
		},
		{
			id: 1028,
			key: "look.fifth_red_tint_percent",
			type: "uint",
			min: 0,
			max: 100,
			default: 35,
			value: 35,
			persist: !0
		},
		{
			id: 1029,
			key: "look.echo_sustain_tint_percent",
			type: "uint",
			min: 0,
			max: 100,
			default: 35,
			value: 35,
			persist: !0
		},
		{
			id: 1030,
			key: "look.echo_secondary_g_percent",
			type: "uint",
			min: 0,
			max: 100,
			default: 18,
			value: 18,
			persist: !0
		},
		{
			id: 1031,
			key: "look.echo_secondary_b_percent",
			type: "uint",
			min: 0,
			max: 100,
			default: 12,
			value: 12,
			persist: !0
		},
		{
			id: 1032,
			key: "look.echo_flash_ms",
			type: "uint",
			min: 0,
			max: 2e3,
			default: 180,
			value: 180,
			persist: !0
		},
		{
			id: 1280,
			key: "midi.din_trs_type",
			type: "enum",
			values: ["a", "b"],
			default: "a",
			value: "a",
			persist: !0
		},
		{
			id: 1536,
			key: "features.melodic_harmonics",
			type: "bool",
			default: !0,
			value: !0,
			persist: !0
		},
		{
			id: 1537,
			key: "features.harmonics.arm_ms",
			type: "uint",
			min: 0,
			max: 1e3,
			default: 150,
			value: 150,
			persist: !0
		},
		{
			id: 1538,
			key: "features.harmonics.confirm_ms",
			type: "uint",
			min: 0,
			max: 500,
			default: 40,
			value: 40,
			persist: !0
		},
		{
			id: 1539,
			key: "features.harmonics.press_depth",
			type: "uint",
			min: 1,
			max: 1e3,
			default: 128,
			value: 128,
			persist: !0
		}
	]
}, x = {
	vendorId: 4617,
	productId: 1,
	productName: "SENTIA TILES (Simulator)",
	manufacturerName: "Sentia Instruments"
}, S = (e) => typeof e == "boolean" ? e ? "1" : "0" : String(e), C = new Map(b.settings.map((e) => [e.key, {
	...e,
	value: S(e.value)
}]));
async function w(e, t = !1) {
	console.log("[SIM]", e);
	let [n, r, ...i] = e.trim().split(/\s+/);
	if (n === "GET") {
		let e = C.get(r);
		return e ? [e.value] : ["ERR unknown-key"];
	}
	if (n === "SET") {
		let e = C.get(r);
		if (!e) return ["ERR unknown-key"];
		let t = i.join(" ");
		if (e.type === "bool" && !["0", "1"].includes(t)) return ["ERR bad-value"];
		if (e.type === "enum" && (!e.values || !e.values.includes(t))) return ["ERR out-of-range"];
		if ([
			"int",
			"uint",
			"float"
		].includes(e.type)) {
			let n = Number(t);
			if (!t || !Number.isFinite(n) || e.type !== "float" && !Number.isInteger(n) || e.type === "uint" && n < 0) return ["ERR bad-value"];
			if (e.min !== void 0 && n < e.min || e.max !== void 0 && n > e.max) return ["ERR out-of-range"];
		}
		return e.value = t, ["OK"];
	}
	return n === "LIST" ? Array.from(C.values()).map((e) => `${e.key}=${e.value}`) : ["ERR unsupported-command"];
}
//#endregion
//#region src/main/index.ts
var T = r(a(import.meta.url)), E = null, D = process.env.TILES_MOCK === "1";
function O() {
	E = new e({
		width: 1200,
		height: 800,
		webPreferences: {
			preload: i(T, "preload.mjs"),
			contextIsolation: !0,
			nodeIntegration: !1
		}
	}), process.env.VITE_DEV_SERVER_URL ? E.loadURL(process.env.VITE_DEV_SERVER_URL) : E.loadFile("dist/index.html");
}
async function k() {
	let e = await p();
	E?.webContents.send("usb:changed", e);
}
t.whenReady().then(() => {
	n.handle("usb:list", async () => D ? [x] : p()), n.handle("usb:connect", async (e, t, n) => D ? {
		connected: !0,
		...x
	} : m(t, n)), n.handle("usb:disconnect", async () => D ? { connected: !1 } : h()), o.addEventListener("connect", () => {
		k();
	}), o.addEventListener("disconnect", () => {
		k();
	}), n.handle("usb:command", async (e, t, n = !1) => D ? w(t, n) : _(t, n)), O();
}), t.on("window-all-closed", () => {
	process.platform !== "darwin" && t.quit();
}), t.on("activate", () => {
	e.getAllWindows().length === 0 && O();
});
//#endregion
export {};
