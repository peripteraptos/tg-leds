<script lang="ts">
  import { FFmpeg } from "@ffmpeg/ffmpeg";
  // @ts-ignore
  import type { LogEvent } from "@ffmpeg/ffmpeg/dist/esm/types";
  import { fetchFile, toBlobURL } from "@ffmpeg/util";
  import { store } from "./store.svelte";
  import { derived } from "svelte/store";
  // const baseURL = 'https://cdn.jsdelivr.net/npm/@ffmpeg/core-mt@0.12.10/dist/esm';
  const baseURL = "/wasm-files";
  // const videoURL =  "https://raw.githubusercontent.com/ffmpegwasm/testdata/master/video-15s.avi";

  const { videoURL, points, radius } = $props();
  console.log("videoURL:", videoURL);
  let isBusy = $state(false);
  let csvOutput = $state("");

  // sampling parameters
  const sampleTimeMs = 100;
  //   const radiusNorm = 0.01; // normalized: 1 = video width
  //   const points = [
  //     { x: 0.4, y: 0.3 },
  //     { x: 0.7, y: 0.2 },
  //     // ...
  //   ];

  async function sampleVideo() {
    if (isBusy) return;
    isBusy = true;
    store.message = "Initializing ffmpeg...";

    const ffmpeg = new FFmpeg();
    ffmpeg.on("log", ({ message: msg }: LogEvent) => {
      console.log(msg);
      store.message = msg;
    });

    console.log("Loading ffmpeg core...");
    // console.log(wasmURL);
    const config = {
      coreURL: await toBlobURL(`${baseURL}/ffmpeg-core.js`, "text/javascript"),
      wasmURL: await toBlobURL(
        `${baseURL}/ffmpeg-core.wasm`,
        "application/wasm"
      ),
      workerURL: await toBlobURL(
        `${baseURL}/ffmpeg-core.worker.js`,
        "text/javascript"
      ),
    };
    console.log("ffmpeg core config:", config);
    await ffmpeg.load(config);

    store.message = "Loading video into ffmpeg FS...";
    await ffmpeg.writeFile("input.avi", await fetchFile(videoURL));

    const fps = 1000 / sampleTimeMs;
    const rows: string[] = ["pointIndex,tMs,r,g,b"];

    // run one ffmpeg command per point
    for (let i = 0; i < points.length; i++) {
      const p = points[i];

      // build the video filter string
      const vf = [
        `fps=${fps}`, // sample rate
        // crop area: width & height = 2 * radiusNorm * iw
        `crop=w=iw*${2 * radius}:h=iw*${2 * radius}:` +
          // center at (nx, ny) in normalized coords
          `x=iw*${p.x}-iw*${radius}:y=ih*${p.y}-iw*${radius}`,
        "scale=1:1", // average to 1x1 pixel
      ].join(",");

      const outName = `point_${i}.rgb`;

      store.message = `Sampling point ${i} with ffmpeg...`;
      // -f rawvideo and -pix_fmt rgb24 give 3 bytes per frame: R,G,B
      await ffmpeg.exec([
        "-i",
        "input.avi",
        "-vf",
        vf,
        "-pix_fmt",
        "rgb24",
        "-f",
        "rawvideo",
        outName,
      ]);

      const data = (await ffmpeg.readFile(outName)) as Uint8Array;
      const frameCount = data.length / 3;

      for (let f = 0; f < frameCount; f++) {
        const idx = f * 3;
        const r = data[idx];
        const g = data[idx + 1];
        const b = data[idx + 2];
        const tMs = f * sampleTimeMs;
        rows.push(`${i},${tMs},${r},${g},${b}`);
      }

      // optional: clean up file
      // await ffmpeg.deleteFile(outName);
    }

    csvOutput = rows.join("\n");
    store.message = "Sampling complete";
    isBusy = false;

    // Download the CSV file
    const blob = new Blob([csvOutput], { type: "text/csv" });
    const url = URL.createObjectURL(blob);
    const a = document.createElement("a");
    a.href = url;
    a.download = "video_sampling_output.csv";
    document.body.appendChild(a);
    a.click();
  }
</script>

<div>
  <button
    on:click={sampleVideo}
    disabled={isBusy}
    class="border rounded p-1 text-center w-full"
  >
    {isBusy ? "Working…" : "Generate sequence CSV"}
  </button>

  <!-- {#if csvOutput}
    <h3>CSV Output</h3>
    <textarea
      readonly
      rows="15"
      style="width: 100%; font-family: monospace;"
      bind:value={csvOutput}
    />
  {/if} -->
</div>
