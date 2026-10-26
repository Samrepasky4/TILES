import { BrowserWindow, app, ipcMain } from "electron";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { usb } from "usb";
//#region src/main/usb.ts
var activeDevice = null;
var controlInterface = null;
var controlIn = null;
var controlOut = null;
var receiveBuffer = "";
var commandQueue = Promise.resolve();
async function listUsbDevices() {
	return (await usb.getDevices()).map((device) => ({
		vendorId: device.vendorId,
		productId: device.productId,
		productName: device.productName ?? void 0,
		manufacturerName: device.manufacturerName ?? void 0,
		serialNumber: device.serialNumber ?? void 0
	}));
}
async function connectUsbDevice(vendorId, productId) {
	await disconnectUsbDevice();
	const device = await usb.findDeviceByIds(vendorId, productId);
	if (!device) throw new Error(`USB device ${hex(vendorId)}:${hex(productId)} not found`);
	await device.open();
	const iface = device.configuration?.interfaces.find((i) => i.alternate.interfaceClass === 255 && i.alternate.interfaceName === "SENTIA TILES Control");
	if (!iface) {
		await device.close();
		throw new Error("SENTIA TILES Control interface not found");
	}
	const out = iface.alternate.endpoints.find((e) => e.direction === "out");
	const input = iface.alternate.endpoints.find((e) => e.direction === "in");
	if (!out || !input) {
		await device.close();
		throw new Error("TILES control endpoints not found");
	}
	await device.claimInterface(iface.interfaceNumber);
	activeDevice = device;
	controlInterface = iface.interfaceNumber;
	controlOut = out.endpointNumber;
	controlIn = input.endpointNumber;
	receiveBuffer = "";
	console.log("TILES ready:", {
		interface: controlInterface,
		outEndpoint: controlOut,
		inEndpoint: controlIn
	});
	return {
		connected: true,
		vendorId: device.vendorId,
		productId: device.productId,
		productName: device.productName,
		manufacturerName: device.manufacturerName
	};
}
async function disconnectUsbDevice() {
	if (activeDevice) try {
		if (controlInterface !== null) await activeDevice.releaseInterface(controlInterface);
		await activeDevice.close();
	} catch {}
	activeDevice = null;
	controlInterface = null;
	controlIn = null;
	controlOut = null;
	receiveBuffer = "";
	return { connected: false };
}
async function readLine() {
	if (!activeDevice || controlIn === null) throw new Error("TILES is not connected");
	while (!receiveBuffer.includes("\n")) {
		const result = await activeDevice.transferIn(controlIn, 64);
		if (result.status !== "ok" || !result.data) throw new Error(`USB read failed: ${result.status}`);
		const data = new Uint8Array(result.data.buffer, result.data.byteOffset, result.data.byteLength);
		receiveBuffer += new TextDecoder().decode(data);
	}
	const index = receiveBuffer.indexOf("\n");
	const line = receiveBuffer.slice(0, index).replace(/\r$/, "");
	receiveBuffer = receiveBuffer.slice(index + 1);
	return line;
}
function sendTilesCommand(command, multiLine = false) {
	const result = commandQueue.then(() => sendTilesCommandNow(command, multiLine));
	commandQueue = result.then(() => void 0, () => void 0);
	return result;
}
async function sendTilesCommandNow(command, multiLine = false) {
	if (!activeDevice || controlOut === null) throw new Error("TILES is not connected");
	console.log(`→ TILES: ${command}`);
	const write = await activeDevice.transferOut(controlOut, new TextEncoder().encode(`${command}\n`));
	if (write.status !== "ok") throw new Error(`USB write failed: ${write.status}`);
	const lines = [];
	while (true) {
		const line = await readLine();
		console.log(`← TILES: ${line}`);
		if (multiLine && line === "OK") return lines;
		lines.push(line);
		if (!multiLine || line.startsWith("ERR ")) return lines;
	}
}
function hex(value) {
	return `0x${value.toString(16).padStart(4, "0")}`;
}
var tiles_settings_default = {
	_comment: "Captured from a real SENTIA TILES (board 2, firmware 0.2.0) with tools/tiles_control.py schema + list on 2026-09-29. For the companion app's fake device -- see ../BRIEF.md. 'value' is that board's current value; the real app must read it from the device.",
	firmware: "0.2.0",
	settings: [
		{
			"id": 256,
			"key": "pedal.mode",
			"type": "enum",
			"values": ["sustain", "expression"],
			"default": "sustain",
			"value": "sustain",
			"persist": true
		},
		{
			"id": 257,
			"key": "pedal.polarity",
			"type": "enum",
			"values": ["normally_open", "normally_closed"],
			"default": "normally_open",
			"value": "normally_open",
			"persist": true
		},
		{
			"id": 258,
			"key": "pedal.sustain_style",
			"type": "enum",
			"values": ["synth", "hold"],
			"default": "synth",
			"value": "synth",
			"persist": true
		},
		{
			"id": 512,
			"key": "expression.mpe_enabled",
			"type": "bool",
			"default": true,
			"value": true,
			"persist": true
		},
		{
			"id": 513,
			"key": "expression.pitch_bend_sensitivity",
			"type": "float",
			"min": .001,
			"max": 1,
			"default": .065,
			"value": .1,
			"persist": true
		},
		{
			"id": 514,
			"key": "expression.aftertouch_sensitivity",
			"type": "uint",
			"min": 1,
			"max": 65535,
			"default": 900,
			"value": 1300,
			"persist": true
		},
		{
			"id": 768,
			"key": "cv_gate.enabled",
			"type": "bool",
			"default": false,
			"value": false,
			"persist": false
		},
		{
			"id": 769,
			"key": "cv_gate.pitch.volts_per_semitone",
			"type": "float",
			"min": .001,
			"max": 1,
			"default": .083333,
			"value": .083333,
			"persist": true
		},
		{
			"id": 770,
			"key": "cv_gate.pitch.reference_note",
			"type": "int",
			"min": 0,
			"max": 127,
			"default": 0,
			"value": 0,
			"persist": true
		},
		{
			"id": 771,
			"key": "cv_gate.pitch.zero_trim_volts",
			"type": "float",
			"min": -2.5,
			"max": 2.5,
			"default": 0,
			"value": 0,
			"persist": true
		},
		{
			"id": 772,
			"key": "cv_gate.pitch.gain_trim",
			"type": "float",
			"min": .5,
			"max": 2,
			"default": 1,
			"value": 1,
			"persist": true
		},
		{
			"id": 773,
			"key": "cv_gate.pressure.full_scale_volts",
			"type": "float",
			"min": .1,
			"max": 10,
			"default": 10,
			"value": 10,
			"persist": true
		},
		{
			"id": 774,
			"key": "cv_gate.pressure.zero_trim_volts",
			"type": "float",
			"min": -2.5,
			"max": 2.5,
			"default": 0,
			"value": 0,
			"persist": true
		},
		{
			"id": 775,
			"key": "cv_gate.pressure.gain_trim",
			"type": "float",
			"min": .5,
			"max": 2,
			"default": 1,
			"value": 1,
			"persist": true
		},
		{
			"id": 1024,
			"key": "look.idle_baseline_percent",
			"type": "uint",
			"min": 0,
			"max": 100,
			"default": 50,
			"value": 50,
			"persist": true
		},
		{
			"id": 1025,
			"key": "look.natural_pad_percent",
			"type": "uint",
			"min": 0,
			"max": 100,
			"default": 21,
			"value": 21,
			"persist": true
		},
		{
			"id": 1026,
			"key": "look.root_pad_percent",
			"type": "uint",
			"min": 0,
			"max": 100,
			"default": 40,
			"value": 40,
			"persist": true
		},
		{
			"id": 1027,
			"key": "look.fifth_pad_percent",
			"type": "uint",
			"min": 0,
			"max": 100,
			"default": 40,
			"value": 40,
			"persist": true
		},
		{
			"id": 1028,
			"key": "look.fifth_red_tint_percent",
			"type": "uint",
			"min": 0,
			"max": 100,
			"default": 35,
			"value": 35,
			"persist": true
		},
		{
			"id": 1029,
			"key": "look.echo_sustain_tint_percent",
			"type": "uint",
			"min": 0,
			"max": 100,
			"default": 35,
			"value": 35,
			"persist": true
		},
		{
			"id": 1030,
			"key": "look.echo_secondary_g_percent",
			"type": "uint",
			"min": 0,
			"max": 100,
			"default": 18,
			"value": 18,
			"persist": true
		},
		{
			"id": 1031,
			"key": "look.echo_secondary_b_percent",
			"type": "uint",
			"min": 0,
			"max": 100,
			"default": 12,
			"value": 12,
			"persist": true
		},
		{
			"id": 1032,
			"key": "look.echo_flash_ms",
			"type": "uint",
			"min": 0,
			"max": 2e3,
			"default": 180,
			"value": 180,
			"persist": true
		},
		{
			"id": 1280,
			"key": "midi.din_trs_type",
			"type": "enum",
			"values": ["a", "b"],
			"default": "a",
			"value": "a",
			"persist": true
		},
		{
			"id": 1536,
			"key": "features.melodic_harmonics",
			"type": "bool",
			"default": true,
			"value": true,
			"persist": true
		},
		{
			"id": 1537,
			"key": "features.harmonics.arm_ms",
			"type": "uint",
			"min": 0,
			"max": 1e3,
			"default": 150,
			"value": 150,
			"persist": true
		},
		{
			"id": 1538,
			"key": "features.harmonics.confirm_ms",
			"type": "uint",
			"min": 0,
			"max": 500,
			"default": 40,
			"value": 40,
			"persist": true
		},
		{
			"id": 1539,
			"key": "features.harmonics.press_depth",
			"type": "uint",
			"min": 1,
			"max": 1e3,
			"default": 128,
			"value": 128,
			"persist": true
		}
	]
};
//#endregion
//#region src/main/mockUsb.ts
var mockDevice = {
	vendorId: 4617,
	productId: 1,
	productName: "SENTIA TILES (Simulator)",
	manufacturerName: "Sentia Instruments"
};
var toWire = (value) => typeof value === "boolean" ? value ? "1" : "0" : String(value);
var settings = new Map(tiles_settings_default.settings.map((s) => [s.key, {
	...s,
	value: toWire(s.value)
}]));
async function mockCommand(command, _multiLine = false) {
	console.log("[SIM]", command);
	const [action, key, ...rest] = command.trim().split(/\s+/);
	if (action === "GET") {
		const setting = settings.get(key);
		return setting ? [setting.value] : ["ERR unknown-key"];
	}
	if (action === "SET") {
		const setting = settings.get(key);
		if (!setting) return ["ERR unknown-key"];
		const value = rest.join(" ");
		if (setting.type === "bool" && !["0", "1"].includes(value)) return ["ERR bad-value"];
		if (setting.type === "enum" && (!setting.values || !setting.values.includes(value))) return ["ERR out-of-range"];
		if ([
			"int",
			"uint",
			"float"
		].includes(setting.type)) {
			const n = Number(value);
			if (!value || !Number.isFinite(n)) return ["ERR bad-value"];
			if (setting.type !== "float" && !Number.isInteger(n) || setting.type === "uint" && n < 0) return ["ERR bad-value"];
			if (setting.min !== void 0 && n < setting.min || setting.max !== void 0 && n > setting.max) return ["ERR out-of-range"];
		}
		setting.value = value;
		return ["OK"];
	}
	if (action === "LIST") return Array.from(settings.values()).map((s) => `${s.key}=${s.value}`);
	return ["ERR unsupported-command"];
}
//#endregion
//#region src/main/index.ts
var __dirname = dirname(fileURLToPath(import.meta.url));
var mainWindow = null;
var isMock = process.env.TILES_MOCK === "1";
function createWindow() {
	mainWindow = new BrowserWindow({
		width: 1200,
		height: 800,
		webPreferences: {
			preload: join(__dirname, "preload.mjs"),
			contextIsolation: true,
			nodeIntegration: false
		}
	});
	if (process.env.VITE_DEV_SERVER_URL) mainWindow.loadURL(process.env.VITE_DEV_SERVER_URL);
	else mainWindow.loadFile("dist/index.html");
}
async function sendUsbList() {
	const devices = await listUsbDevices();
	mainWindow?.webContents.send("usb:changed", devices);
}
app.whenReady().then(() => {
	ipcMain.handle("usb:list", async () => {
		if (isMock) return [mockDevice];
		return listUsbDevices();
	});
	ipcMain.handle("usb:connect", async (_event, vendorId, productId) => {
		if (isMock) return {
			connected: true,
			...mockDevice
		};
		return connectUsbDevice(vendorId, productId);
	});
	ipcMain.handle("usb:disconnect", async () => {
		if (isMock) return { connected: false };
		return disconnectUsbDevice();
	});
	usb.addEventListener("connect", () => {
		sendUsbList();
	});
	usb.addEventListener("disconnect", () => {
		sendUsbList();
	});
	ipcMain.handle("usb:command", async (_event, command, multiLine = false) => {
		if (isMock) return mockCommand(command, multiLine);
		return sendTilesCommand(command, multiLine);
	});
	createWindow();
});
app.on("window-all-closed", () => {
	if (process.platform !== "darwin") app.quit();
});
app.on("activate", () => {
	if (BrowserWindow.getAllWindows().length === 0) createWindow();
});
//#endregion
export {};
