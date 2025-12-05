<script lang="ts">
  import { onMount } from "svelte";
  //   import FFmpegDemo from "./FFmpegDemo.svelte";
  import { store } from "./store.svelte";
  import Button from "./Button.svelte";

  interface Color {
    r: number;
    g: number;
    b: number;
  }

  interface Point {
    x: number;
    y: number;
    color: Color;
  }

  interface Config {
    pointRadius: number;
    points: Point[];
  }

  const tools = {
    pointer: {
      label: "Move Points",
      cursor: "",
      pointCursor: "cursor-grabbing",
    },
    add: {
      label: "Add Points",
      cursor: "cursor-crosshair",
      pointCursor: "cursor-crosshair",
    },
    remove: {
      label: "Remove Points",
      cursor: "",
      pointCursor: "cursor-not-allowed",
    },
  };

  let currentTool = $state<keyof typeof tools>("pointer");

  const STORAGE_KEY = "videoPointsConfig";

  let videoEl = $state<HTMLVideoElement>();
  let canvasEl = $state<HTMLCanvasElement>();
  let ctx: CanvasRenderingContext2D | null = null;

  let videoUrl = $state<string | null>(
    "http://commondatastorage.googleapis.com/gtv-videos-bucket/sample/BigBuckBunny.mp4"
  );

  let defaultConfig: Config = {
    pointRadius: 0.05,
    points: [
      { x: 0.05875, y: 0.07333333333333333, color: { r: 97, g: 109, b: 59 } },
      { x: 0.12625, y: 0.14666666666666667, color: { r: 147, g: 140, b: 88 } },
      { x: 0.2075, y: 0.21777777777777776, color: { r: 86, g: 117, b: 135 } },
      { x: 0.27375, y: 0.27555555555555555, color: { r: 70, g: 114, b: 139 } },
      { x: 0.3275, y: 0.3377777777777778, color: { r: 13, g: 30, b: 29 } },
      { x: 0.41, y: 0.4266666666666667, color: { r: 5, g: 8, b: 18 } },
      { x: 0.48625, y: 0.4866666666666667, color: { r: 15, g: 26, b: 32 } },
      { x: 0.545, y: 0.5377777777777778, color: { r: 27, g: 36, b: 42 } },
      { x: 0.61375, y: 0.6066666666666667, color: { r: 14, g: 26, b: 33 } },
      { x: 0.7125, y: 0.6933333333333334, color: { r: 6, g: 13, b: 16 } },
      { x: 0.795, y: 0.7711111111111111, color: { r: 255, g: 255, b: 121 } },
      { x: 0.8575, y: 0.8488888888888889, color: { r: 235, g: 222, b: 85 } },
    ] as Point[],
  };
  for (const [index, p] of defaultConfig.points.entries()) {
    p.x = index * (1 / defaultConfig.points.length) + 0.05;
    p.y = index * (1 / defaultConfig.points.length) + 0.05;
  }

  let config = $state(structuredClone(defaultConfig));

  let animationFrameId = 0;

  let loaded = $state(false);
  // dragging state
  let isDragging = $state(false);
  let dragIndex: number | null = null;
  let dragOffsetX = $state(0);
  let dragOffsetY = $state(0);
  let hasDragged = $state(false);
  let duration = $state(0);
  let currentTime = $state(0);
  let isPlaying = $state(false);
  let isCapturing = $state(false);
  let captureData: Record<string, ({ time: number } & Color)[]> = $state({});

  onMount(() => {
    if (!canvasEl) return;
    ctx = canvasEl.getContext("2d");

    // load points from localStorage
    try {
      const stored = localStorage.getItem(STORAGE_KEY);
      if (stored) {
        const parsed = JSON.parse(stored) as Config;
        console.log("Loaded stored points", parsed);
        config = parsed;
        config.points = parsed.points.map((p) => ({
          x: p.x,
          y: p.y,
          color: { r: 255, g: 255, b: 255 },
        }));
      }
      loaded = true;
    } catch (e) {
      console.error("Failed to parse stored points", e);
    }

    const handleResize = () => {
      if (!videoEl || !canvasEl) return;
      const rect = videoEl.getBoundingClientRect();
      canvasEl.width = rect.width;
      canvasEl.height = rect.height;
    };

    handleResize();
    window.addEventListener("resize", handleResize);

    const onLoadedMetadata = () => {
      handleResize();
    };

    const onPlay = () => {
      isPlaying = true;
    };

    const onPause = () => {
      isPlaying = false;
    };

    videoEl?.addEventListener("loadedmetadata", onLoadedMetadata);
    videoEl?.addEventListener("play", onPlay);
    videoEl?.addEventListener("pause", onPause);

    animationFrameId = requestAnimationFrame(renderFrame);

    return () => {
      window.removeEventListener("resize", handleResize);
      videoEl?.removeEventListener("loadedmetadata", onLoadedMetadata);
      videoEl?.removeEventListener("play", onPlay);
      videoEl?.removeEventListener("pause", onPause);
    };
  });

  //   persist points whenever they change
  $effect(() => {
    loaded && localStorage.setItem(STORAGE_KEY, JSON.stringify(config));
  });

  function renderFrame() {
    if (!ctx || !canvasEl || !videoEl) return;

    currentTime = videoEl.currentTime;

    const width = canvasEl.width;
    const height = canvasEl.height;

    ctx.clearRect(0, 0, width, height);
    ctx.drawImage(videoEl, 0, 0, width, height);

    if (config.points.length) {
      const updated: Point[] = [];

      for (const [index, p] of config.points.entries()) {
        const color = sampleAverageColor(
          denormalize(p.x, width),
          denormalize(p.y, height),
          denormalize(config.pointRadius, width)
        );
        updated.push({
          ...p,
          color,
        });
        if (isCapturing) {
          const lightName = "led-" + (index + 1);

          // store color data
          if (!captureData[lightName]) {
            captureData[lightName] = [];
          }
          // console.log(
          //   `Capture point ${index} at time ${videoEl.currentTime.toFixed(
          //     2
          //   )}: rgb(${p.color.r}, ${p.color.g}, ${p.color.b})`
          // );
          if (
            captureData[lightName].find(
              (d) => d.time === Math.round(videoEl!.currentTime * 1000)
            )
          ) {
            // already have data for this time
            console.log(
              `Skipping duplicate sample for ${lightName} at time ${Math.round(
                videoEl.currentTime * 1000
              )} ms`
            );
            continue;
          }

          if (captureData[lightName] && captureData[lightName].length > 0) {
            const lastEntry =
              captureData[lightName][captureData[lightName].length - 1];
            if (
              lastEntry &&
              lastEntry.r == color.r &&
              lastEntry.g == color.g &&
              lastEntry.b == color.b
            ) {
              // skip samples that are too close in time
              // console.log(
              //   `Skipping similar sample for ${lightName} at time ${Math.round(
              //     videoEl.currentTime * 1000
              //   )} ms`
              // );
              continue;
            }
          }
          captureData[lightName].push({
            time: Math.round(videoEl.currentTime * 1000),
            ...color,
          });
        }
      }
      config.points = updated;

      // draw circles on top
      for (const [, p] of config.points.entries()) {
        ctx.beginPath();
        ctx.arc(
          denormalize(p.x, width),
          denormalize(p.y, height),
          denormalize(config.pointRadius, width),
          0,
          Math.PI * 2
        );
        ctx.fillStyle = "rgba(0,255,0,0.2)";
        ctx.strokeStyle = "#00ff00";
        ctx.lineWidth = 2;
        ctx.fill();
        ctx.stroke();
      }
    }

    animationFrameId = requestAnimationFrame(renderFrame);
  }

  // playback video from beginning to end at high speed and capture colors
  async function playbackAndCapture() {
    if (!videoEl) return;
    videoEl.currentTime = 0;
    videoEl.playbackRate = 20.0;
    videoEl.loop = false;
    captureData = {};
    return await new Promise<void>((resolve, reject) => {
      videoEl?.pause();

      videoEl?.addEventListener(
        "seeked",
        () => {
          setTimeout(() => {
            videoEl?.addEventListener(
              "ended",
              () => {
                resolve();
                isCapturing = false;
                if (videoEl) {
                  videoEl.playbackRate = 1.0;
                  videoEl.loop = true;
                }
              },
              { once: true }
            );

            videoEl?.play().then(() => {
              isCapturing = true;
              console.log("Video playback started for capture");
            });
          }, 100);
        },
        { once: true }
      );
    });

    videoEl!.currentTime = 0;
  }

  // download captured data as JSON
  function downloadCaptureData() {
    const processed = Object.fromEntries(
      Object.entries(captureData).map(([key, samples]) => {
        // sort samples by time
        const sorted = samples.slice().sort((a, b) => a.time - b.time);
        const mapped = sorted.map((s, index, arr) => {
          // transform to delta time
          if (index === 0) {
            return s;
          } else {
            return {
              ...s,
              time: s.time - arr[index - 1].time,
            };
          }
        });
        const filtered = mapped.filter((s, index, arr) => {
          // remove samples without changes
          if (index === 0) return true;
          const prev = arr[index - 1];
          return s.r !== prev.r || s.g !== prev.g || s.b !== prev.b;
        });
        return [key, filtered];
      })
    );
    const dataStr = JSON.stringify(processed, null, 2);
    const blob = new Blob([dataStr], { type: "application/json" });
    const url = URL.createObjectURL(blob);
    const a = document.createElement("a");
    a.href = url;
    a.download = "samples.json";
    document.body.appendChild(a);
    a.click();
    document.body.removeChild(a);
    URL.revokeObjectURL(url);
  }

  // BONUS: average pixels inside a circle of radius pintRadius
  function sampleAverageColor(x: number, y: number, radius: number): Color {
    if (!ctx || !canvasEl) return { r: 0, g: 0, b: 0 };

    const r = Math.max(1, Math.round(radius));
    const startX = Math.max(0, Math.floor(x - r));
    const endX = Math.min(canvasEl.width - 1, Math.ceil(x + r));
    const startY = Math.max(0, Math.floor(y - r));
    const endY = Math.min(canvasEl.height - 1, Math.ceil(y + r));

    const w = endX - startX + 1;
    const h = endY - startY + 1;

    const imageData = ctx.getImageData(startX, startY, w, h);
    const data = imageData.data;

    let sumR = 0;
    let sumG = 0;
    let sumB = 0;
    let count = 0;

    for (let yy = startY; yy <= endY; yy++) {
      for (let xx = startX; xx <= endX; xx++) {
        const dx = xx - x;
        const dy = yy - y;
        if (dx * dx + dy * dy <= r * r) {
          const idx = ((yy - startY) * w + (xx - startX)) * 4;
          sumR += data[idx];
          sumG += data[idx + 1];
          sumB += data[idx + 2];
          count++;
        }
      }
    }

    if (count === 0) {
      const p = ctx.getImageData(x, y, 1, 1).data;
      return {
        r: p[0],
        g: p[1],
        b: p[2],
      };
    }

    const avgR = Math.round(sumR / count);
    const avgG = Math.round(sumG / count);
    const avgB = Math.round(sumB / count);

    return { r: avgR, g: avgG, b: avgB };
  }

  // --- canvas interactions ---

  function canvasClick(event: MouseEvent) {
    event.preventDefault();
    event.stopPropagation();

    // prevent "click" after drag from creating a new point

    switch (currentTool) {
      case "pointer":
        return; // do nothing
      case "add":
        addPointAtEvent(event);
        break;
      case "remove":
        return;
        // removePointAtEvent(event);
        break;
    }
  }

  function addPointAtEvent(event: MouseEvent) {
    const rect = canvasEl?.getBoundingClientRect();
    if (!rect) return;
    const x = normalize(event.clientX - rect.left, rect.width);
    const y = normalize(event.clientY - rect.top, rect.height);
    config.points = [
      ...config.points,
      {
        x,
        y,
        color: { r: 0, g: 0, b: 0 },
      },
    ];
  }

  function denormalize(value: number, scale: number) {
    return value * scale;
  }
  function normalize(value: number, scale: number) {
    return value / scale;
  }

  function pointPointerDown(event: PointerEvent) {
    event.stopPropagation();
    if (currentTool === "add") return;
    // console.log("point pointer down");
    const target = event.currentTarget as HTMLElement;
    const index = Number(target.textContent) - 1;

    if (index === -1) return;

    if (currentTool === "remove") {
      config.points.splice(index, 1);
      return;
    }

    dragIndex = index;
    isDragging = true;
    hasDragged = false;
    const rect = canvasEl?.getBoundingClientRect();
    if (!rect) return;
    const x = event.clientX - rect.left;
    const y = event.clientY - rect.top;
    dragOffsetX = config.points[index].x - normalize(x, rect.width);
    dragOffsetY = config.points[index].y - normalize(y, rect.height);

    videoEl?.setPointerCapture(event.pointerId);
    event.preventDefault();
  }

  function canvasPointerDown(event: PointerEvent) {
    // console.log("pointer down", event);
    const rect = canvasEl?.getBoundingClientRect();
    if (!rect) return;
    const x = event.clientX - rect.left;
    const y = event.clientY - rect.top;

    if (currentTool !== "pointer") return;

    let foundIndex: number | null = null;
    for (let i = 0; i < config.points.length; i++) {
      const p = config.points[i];
      const dx = denormalize(p.x, rect.width) - x;
      const dy = denormalize(p.y, rect.height) - y;
      if (
        dx * dx + dy * dy <=
        denormalize(config.pointRadius, rect.width) ** 2
      ) {
        foundIndex = i;
        break;
      }
    }
    if (foundIndex !== null) {
      dragIndex = foundIndex;
      isDragging = true;
      hasDragged = false;
      dragOffsetX = config.points[foundIndex].x - normalize(x, rect.width);
      dragOffsetY = config.points[foundIndex].y - normalize(y, rect.height);
      videoEl?.setPointerCapture(event.pointerId);
      event.preventDefault();
    }
  }

  function canvasPointerMove(event: PointerEvent) {
    if (!isDragging || dragIndex === null) return;

    const rect = canvasEl?.getBoundingClientRect();
    if (!rect) return;
    let x = normalize(event.clientX - rect.left, rect.width);
    let y = normalize(event.clientY - rect.top, rect.height);
    x = Math.max(0, Math.min(1, x));
    y = Math.max(0, Math.min(1, y));

    const newX = x + dragOffsetX;
    const newY = y + dragOffsetY;

    hasDragged = true;

    // console.log("dragging to", dragIndex, newX, newY);
    config.points = config.points.map((p, i) =>
      i === dragIndex ? { ...p, x: newX, y: newY } : p
    );
  }

  function canvasPointerUp(event: PointerEvent) {
    if (isDragging) {
      config.points = config.points.map((p) => ({
        ...p,
        x: Math.max(0, Math.min(1, p.x)),
        y: Math.max(0, Math.min(1, p.y)),
      }));
      isDragging = false;
      dragIndex = null;
      videoEl?.releasePointerCapture(event.pointerId);
    }
  }

  function canvasMouseLeave(event: MouseEvent) {
    if (isDragging) {
      isDragging = false;
      dragIndex = null;
    }
  }

  // --- buttons: reset / export / import ---

  function resetConfig() {
    config = structuredClone(defaultConfig);
    localStorage.removeItem(STORAGE_KEY);
  }

  function exportPoints() {
    const data = localStorage.getItem(STORAGE_KEY) ?? JSON.stringify(config);

    const blob = new Blob([data], { type: "application/json" });
    const url = URL.createObjectURL(blob);
    const a = document.createElement("a");
    a.href = url;
    a.download = "points.json";
    document.body.appendChild(a);
    a.click();
    document.body.removeChild(a);
    URL.revokeObjectURL(url);
  }

  function onImportFileChange(event: Event) {
    const input = event.currentTarget as HTMLInputElement;
    const file = input.files?.[0];
    if (!file) return;

    const reader = new FileReader();
    reader.onload = (e) => {
      try {
        const text = String(e.target?.result);
        const parsed = JSON.parse(text) as {
          points: {
            id?: number;
            x: number;
            y: number;
          }[];
        };
        config.points = parsed.points.map((p) => ({
          x: p.x,
          y: p.y,
          color: { r: 0, g: 0, b: 0 },
        }));
      } catch (err) {
        console.error("Failed to import points JSON", err);
        alert("Failed to import points JSON.");
      }
    };
    reader.readAsText(file);

    // allow re-importing the same file
    input.value = "";
  }

  // --- video file input ---

  function onVideoFileChange(event: Event) {
    const input = event.currentTarget as HTMLInputElement;
    const file = input.files?.[0];
    if (!file) return;

    if (videoUrl) {
      URL.revokeObjectURL(videoUrl);
    }
    videoUrl = URL.createObjectURL(file);

    if (videoEl) {
      const source = videoEl.querySelector("source");
      if (source) {
        source.src = videoUrl;
        videoEl.load();
        videoEl.play();
      }
    }
  }

  function onCanPlay() {
    if (videoEl) {
      console.log("video loaded, duration:", videoEl.duration);
      duration = videoEl.duration;
    }
  }
</script>

<div class="grid lg:grid-cols-[3fr_3fr] gap-5">
  <div class="flex flex-col">
    <div>
      <label class="flex items-center gap-2 mb-2">
        <span class="font-semibold">Tool:</span>
        <select
          bind:value={currentTool}
          class="bg-neutral-700 text-neutral-300 p-1 rounded"
        >
          {#each Object.entries(tools) as [key, tool]}
            <option value={key}>{tool.label}</option>
          {/each}
        </select>
      </label>
    </div>
    <div>
      <div class="relative overflow-clip">
        <video
          bind:this={videoEl}
          class="w-full z-10 relative {tools[currentTool].cursor}"
          autoplay
          disablePictureInPicture
          crossorigin="anonymous"
          muted
          loop
          oncanplay={onCanPlay}
          onclick={canvasClick}
          onpointerdown={canvasPointerDown}
          onpointermove={canvasPointerMove}
          onpointerup={canvasPointerUp}
          onmouseleave={canvasMouseLeave}
        >
          <!-- fallback default video if you want one -->
          <source src={videoUrl} type="video/mp4" />
          Your browser does not support the video tag.
        </video>

        <canvas
          class="absolute inset-0 -z-10 pointer-events-none"
          bind:this={canvasEl}
        ></canvas>

        {#each config.points as point, key}
          <div
            onpointerdown={pointPointerDown}
            class="absolute z-20 bg-blue-500/60 text-white font-mono px-1 select-none size-10 rounded-full flex items-center justify-center {tools[
              currentTool
            ].pointCursor}"
            style={`left: ${point.x * 100}%; top: ${point.y * 100}%; transform: translate(-50%, -50%) scale(${config.pointRadius * 40});`}
          >
            {key + 1}
          </div>
        {/each}

        {#if videoUrl === null}
          <div
            class="absolute inset-0 flex items-center justify-center z-20 bg-neutral-100"
          >
            <span class="text-gray-500">No video loaded</span>
          </div>
        {/if}
      </div>
    </div>
    {#if videoEl}<div id="controls" class="flex items-center gap-2 mt-2 grow">
        {#if !isPlaying}<Button
            class="w-20"
            onclick={() => {
              videoEl?.play();
            }}>Play</Button
          >{:else}
          <Button
            class="w-20"
            onclick={() => {
              videoEl?.pause();
            }}>Pause</Button
          >{/if}
        <div class="bg-neutral-700 rounded p-1 w-full flex items-center">
          <input
            type="range"
            class="w-full"
            value={currentTime}
            max={duration}
            oninput={(e) => {
              const val = Number((e.target as HTMLInputElement).value);
              if (videoEl) {
                videoEl.currentTime = val;
              }
            }}
          />
        </div>
      </div>{/if}
  </div>
  <div
    class="bg-neutral-800 p-4 rounded-lg flex flex-col gap-4 text-neutral-400 dark"
  >
    <label class="flex flex-col gap-1">
      <span class="font-semibold">Video</span>
      <input
        class="hidden"
        type="file"
        accept="video/*"
        onchange={onVideoFileChange}
      />
      <Button as="div">Load video</Button>
    </label>

    <label class="flex flex-col gap-1">
      <span class="font-semibold"
        >Radius: {(config.pointRadius * 100).toFixed(1)} % width</span
      >
      <input
        type="range"
        min="0.001"
        max="0.1"
        step="0.001"
        bind:value={config.pointRadius}
      />
    </label>
    <label class="flex flex-col gap-1">
      <span class="font-semibold">Reset</span>
      <Button onclick={resetConfig}>Reset config</Button>
    </label>
    <div class="flex flex-col gap-2">
      <span class="font-semibold">Import/Export</span>
      <Button onclick={exportPoints}>Export points</Button>
      <label class="flex flex-col gap-1">
        <input
          class="hidden"
          type="file"
          accept="application/json"
          onchange={onImportFileChange}
        />
        <Button as="div">Import points</Button>
      </label>
      <!-- <FFmpegDemo
        videoURL={videoUrl}
        points={config.points}
        radius={config.pointRadius}
      /> -->
    </div>
    <div class="flex flex-col gap-2">
      <span class="font-semibold">Capture Colors</span>
      <Button
        onclick={async () => {
          await playbackAndCapture();
          console.log("Capture data:", captureData);
        }}
      >
        {isCapturing ? "Capturing..." : "Start Capture"}
      </Button>
      {#if captureData}
        <Button onclick={downloadCaptureData}>Download Captured Data</Button>
      {/if}
    </div>
  </div>
  <p>{store.message}</p>
</div>

{#if !isCapturing}
  <div class="flex gap-6 bg-black py-10 justify-around px-10 grow items-center">
    {#each config.points as point, key}
      <div
        class="grow rounded-full aspect-square max-h-62 max-w-62"
        style={`background-color: rgb(${point.color.r}, ${point.color.g}, ${point.color.b})`}
      ></div>
    {/each}
  </div>
{/if}
