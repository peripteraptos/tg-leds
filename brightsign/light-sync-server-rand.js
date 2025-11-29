// light-sync-server.js
//
// Simple TCP server for the ESPHome LightSync component.
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

import net from "node:net"

// --- Configuration ---------------------------------------------------------

const PORT = 4242;

// Length of the sequence in seconds
const TOTAL_LENGTH_S = 10; // 10 second loop

// Number of samples we send in SEQUENCE
const NUM_SAMPLES = 100;

// How often to send SYNC (ms)
const SYNC_INTERVAL_MS = 4000;

// --- Sequence generation ---------------------------------------------------

// Example: simple RGB chase over the SEQ duration
function generateSequence() {
    const samples = [];
    for (let i = 0; i < NUM_SAMPLES; i++) {
        const t = (TOTAL_LENGTH_S * i) / (NUM_SAMPLES - 1); // 0..TOTAL_LENGTH_S
        const phase = t / TOTAL_LENGTH_S; // 0..1

        // quick and dirty RGB pattern
        const r = Math.max(0, Math.sin(2 * Math.PI * (phase + 0 / 3)));
        const g = Math.max(0, Math.sin(2 * Math.PI * (phase + 1 / 3)));
        const b = Math.max(0, Math.sin(2 * Math.PI * (phase + 2 / 3)));

        samples.push({ t, r, g, b });
    }
    return samples;
}

const SEQUENCE = generateSequence();

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
    console.log('New client connected from', socket.remoteAddress, socket.remotePort);

    socket.setEncoding('utf8');

    let buf = '';
    let lightId = null;
    let syncTimer = null;

    // Handle incoming data (line-based)
    socket.on('data', async (chunk) => {
        buf += chunk;
        let idx;
        while ((idx = buf.indexOf('\n')) !== -1) {
            const line = buf.slice(0, idx);
            buf = buf.slice(idx + 1);
            await handleLine(line.trim());
        }
    });

    socket.on('close', () => {
        console.log('Client disconnected', lightId ? `(${lightId})` : '');
        if (syncTimer) clearInterval(syncTimer);
    });

    socket.on('error', (err) => {
        console.error('Socket error:', err.message);
        if (syncTimer) clearInterval(syncTimer);
    });

    async function handleLine(line) {
        if (!line) return;
        console.log('<=', line);

        if (line.startsWith('HELLO ')) {
            lightId = line.substring(6).trim();
            console.log('Client says HELLO, id =', lightId);

            // Send sequence once
            await sendSequence();

            // Start periodic SYNCs
            if (!syncTimer) {
                syncTimer = setInterval(() => {
                    sendSync();
                }, SYNC_INTERVAL_MS);
            }
        } else {
            console.log('Unknown command from client:', line);
        }
    }

    function writeLine(line) {
        console.log('=>', line);
        socket.write(line + '\n');
    }

    async function sendSequence() {
        writeLine(`SEQLEN ${TOTAL_LENGTH_S.toFixed(3)}`);
        for (const sample of SEQUENCE) {

            // ensure limited decimals to keep lines short
            const t = sample.t.toFixed(3);
            const r = sample.r.toFixed(3);
            const g = sample.g.toFixed(3);
            const b = sample.b.toFixed(3);
            writeLine(`SAMPLE ${t} ${r} ${g} ${b}`);
            await new Promise(resolve => setTimeout(resolve, 50));
        }
        writeLine('SEQEND');
    }

    function sendSync() {
        const t = getCurrentSequenceTime();
        writeLine(`SYNC ${t.toFixed(3)}`);
    }
});

server.listen(PORT, () => {
    console.log(`Light sync server listening on port ${PORT}`);
});
