// light-sync-server.js
//
// Simple TCP server for the ESPHome LightSync component,
// but now loading the sequence from a CSV file:
//
// CSV format:
//   pointIndex,tMs,r,g,b
//   0,0,0,0,0
//   0,100,0,0,0
//   ...
//
// tMs is time in milliseconds from the start of the sequence.
// The protocol still uses seconds for SEQLEN / SAMPLE / SYNC.
//
// Protocol (per connection):
//   Client -> "HELLO <light_id>\n"
//   Server -> "SEQLEN <seconds>\n"
//              "SAMPLE <t> <r> <g> <b>\n"  (multiple lines)
//              ...
//              "SEQEND\n"
//   Then, periodically:
//              "SYNC <time_s>\n"
//
// time_s is in seconds, looping over [0, total_length_s).

import net from "node:net";
import fs from "node:fs";
import path from "node:path";

// --- Configuration ---------------------------------------------------------

const PORT = 4242;

// CSV file path: use CLI arg or default to ./sequence.csv
const CSV_FILE = process.argv[2] || path.join(process.cwd(), "sequence.csv");

// How often to send SYNC (ms)
const SYNC_INTERVAL_MS = 5000;

// --- Sequence loading from CSV --------------------------------------------

function loadSequenceFromCsv(filePath) {
    console.log(`Loading sequence from CSV: ${filePath}`);

    const text = fs.readFileSync(filePath, "utf8");
    const lines = text
        .split(/\r?\n/)
        .map((l) => l.trim())
        .filter((l) => l.length > 0);

    if (lines.length <= 1) {
        throw new Error("CSV appears to be empty or only contains a header row.");
    }

    // Remove header line
    const header = lines.shift();
    console.log("CSV header:", header);

    const samples = [];
    let lastTMs = 0;

    for (const line of lines) {
        const parts = line.split(",");
        if (parts.length < 5) {
            console.warn("Skipping malformed line:", line);
            continue;
        }

        const pointIndex = parseInt(parts[0]); // not used currently
        const tMs = parseFloat(parts[1]);
        const rRaw = parseFloat(parts[2]);
        const gRaw = parseFloat(parts[3]);
        const bRaw = parseFloat(parts[4]);

        if (Number.isNaN(tMs)) {
            console.warn("Skipping line with invalid tMs:", line);
            continue;
        }

        const t = tMs / 1000.0; // convert ms -> seconds

        // If your LightSync code expects 0–1, scale here:
        const r = rRaw / 255.0;
        const g = gRaw / 255.0;
        const b = bRaw / 255.0;

        // const r = rRaw;
        // const g = gRaw;
        // const b = bRaw;

        samples.push({ pointIndex, t, r, g, b });
        lastTMs = tMs;
    }

    if (samples.length === 0) {
        throw new Error("No valid samples found in CSV.");
    }

    const totalLengthS = lastTMs / 1000.0;
    console.log(
        `Loaded ${samples.length} samples, total length ~${totalLengthS.toFixed(
            3,
        )} s`,
    );

    return { samples, totalLengthS };
}

let SEQUENCE;
let TOTAL_LENGTH_S;

try {
    const { samples, totalLengthS } = loadSequenceFromCsv(CSV_FILE);
    SEQUENCE = samples;
    TOTAL_LENGTH_S = totalLengthS;
} catch (err) {
    console.error("Failed to load CSV sequence:", err);
    process.exit(1);
}

const NUM_SAMPLES = SEQUENCE.length;

// Global time (shared for all clients), so all lights stay in sync
const serverStartTime = Date.now();
function getCurrentSequenceTime() {
    const elapsedMs = Date.now() - serverStartTime;
    const elapsedS = elapsedMs / 1000;
    let t = elapsedS % TOTAL_LENGTH_S;
    if (t < 0) t += TOTAL_LENGTH_S;
    return t;
}

// --- Server ----------------------------------------------------------------

const server = net.createServer((socket) => {
    console.log(
        "New client connected from",
        socket.remoteAddress,
        socket.remotePort,
    );

    socket.setEncoding("utf8");

    let buf = "";
    let lightId = null;
    let syncTimer = null;

    // Handle incoming data (line-based)
    socket.on("data", (chunk) => {
        buf += chunk;
        let idx;
        while ((idx = buf.indexOf("\n")) !== -1) {
            const line = buf.slice(0, idx);
            buf = buf.slice(idx + 1);
            handleLine(line.trim());
        }
    });

    socket.on("close", () => {
        console.log("Client disconnected", lightId ? `(${lightId})` : "");
        if (syncTimer) clearInterval(syncTimer);
    });

    socket.on("error", (err) => {
        console.error("Socket error:", err.message);
        if (syncTimer) clearInterval(syncTimer);
    });

    function handleLine(line) {
        if (!line) return;
        console.log("<=", line);

        if (line.startsWith("HELLO ")) {
            lightId = line.substring(6).trim();
            console.log("Client says HELLO, id =", lightId);

            // Send sequence once
            sendSequence(lightId);

            // Start periodic SYNCs
            if (!syncTimer) {
                syncTimer = setInterval(() => {
                    sendSync();
                }, SYNC_INTERVAL_MS);
            }
        } else {
            console.log("Unknown command from client:", line);
        }
    }

    function writeLine(line) {
        console.log("=>", line);
        socket.write(line + "\n");
    }

    function sendSequence(lightId) {
        writeLine(`SEQLEN ${TOTAL_LENGTH_S.toFixed(3)}`);
        for (const sample of SEQUENCE.filter(s => s.pointIndex == lightId)) {
            // sample.t is already in seconds
            const t = sample.t.toFixed(3);
            const r = sample.r.toFixed(3);
            const g = sample.g.toFixed(3);
            const b = sample.b.toFixed(3);
            writeLine(`SAMPLE ${t} ${r} ${g} ${b}`);
        }
        writeLine("SEQEND");
    }

    function sendSync() {
        const t = getCurrentSequenceTime();
        writeLine(`SYNC ${t.toFixed(3)}`);
    }
});

server.listen(PORT, () => {
    console.log(
        `Light sync server listening on port ${PORT}, using CSV: ${CSV_FILE}`,
    );
});
