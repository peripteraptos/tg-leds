<script lang="ts">
  import { onMount } from "svelte";
  import FFmpegDemo from "./FFmpegDemo.svelte";
  import { store } from "./store.svelte";

  interface Point {
    id: number;
    x: number;
    y: number;
    color: string;
  }

  interface Config {
    pointRadius: number;
    points: Point[];
  }

  const STORAGE_KEY = "videoPoints";

  let videoEl: HTMLVideoElement;
  let canvasEl: HTMLCanvasElement;
  let ctx: CanvasRenderingContext2D | null = null;

  let nextId = 1;
  let videoUrl: string | null =
    "http://commondatastorage.googleapis.com/gtv-videos-bucket/sample/BigBuckBunny.mp4";

  let config: Config = {
    pointRadius: 0.01,
    points: [] as Point[],
  };

  let animationFrameId = 0;

  let loaded = false;
  // dragging state
  let isDragging = false;
  let dragIndex: number | null = null;
  let dragOffsetX = 0;
  let dragOffsetY = 0;
  let hasDragged = false;
  let justDraggedClick = false;
  let duration = 0;
  let currentTime = 0;
  let isPlaying = false;

  onMount(() => {
    if (!canvasEl) return;
    ctx = canvasEl.getContext("2d");

    // load points from localStorage
    try {
      const stored = localStorage.getItem(STORAGE_KEY);
      if (stored) {
        const parsed = JSON.parse(stored) as Config;
        config = parsed;
        config.points = parsed.points.map((p, idx) => ({
          id: p.id ?? idx + 1,
          x: p.x,
          y: p.y,
          color: "rgb(255,255,255)",
        }));
        nextId = config.points.reduce((max, p) => Math.max(max, p.id), 0) + 1;
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
      cancelAnimationFrame(animationFrameId);
      renderFrame();
      isPlaying = true;
    };

    const onPause = () => {
      cancelAnimationFrame(animationFrameId);
      isPlaying = false;
    };

    videoEl.addEventListener("loadedmetadata", onLoadedMetadata);
    videoEl.addEventListener("play", onPlay);
    videoEl.addEventListener("pause", onPause);

    return () => {
      window.removeEventListener("resize", handleResize);
      videoEl.removeEventListener("loadedmetadata", onLoadedMetadata);
      videoEl.removeEventListener("play", onPlay);
      videoEl.removeEventListener("pause", onPause);
      cancelAnimationFrame(animationFrameId);
    };
  });

  //   persist points whenever they change
  $: loaded && localStorage.setItem(STORAGE_KEY, JSON.stringify(config));

  function renderFrame() {
    if (!ctx || !canvasEl || !videoEl) return;

    currentTime = videoEl.currentTime;

    const width = canvasEl.width;
    const height = canvasEl.height;

    ctx.clearRect(0, 0, width, height);
    ctx.drawImage(videoEl, 0, 0, width, height);

    if (config.points.length) {
      const updated: Point[] = [];

      for (const p of config.points) {
        const color = sampleAverageColor(
          denormalize(p.x, width),
          denormalize(p.y, height),
          denormalize(config.pointRadius, width)
        );
        updated.push({ ...p, color });
      }
      config.points = updated;

      // draw circles on top
      for (const p of config.points) {
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

    if (!videoEl.paused && !videoEl.ended) {
      animationFrameId = requestAnimationFrame(renderFrame);
    }
  }

  // BONUS: average pixels inside a circle of radius pointRadius
  function sampleAverageColor(x: number, y: number, radius: number): string {
    if (!ctx || !canvasEl) return "rgb(0,0,0)";

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
      return `rgb(${p[0]}, ${p[1]}, ${p[2]})`;
    }

    const avgR = Math.round(sumR / count);
    const avgG = Math.round(sumG / count);
    const avgB = Math.round(sumB / count);

    return `rgb(${avgR}, ${avgG}, ${avgB})`;
  }

  // --- canvas interactions ---

  function canvasClick(event: MouseEvent) {
    // prevent "click" after drag from creating a new point
    if (justDraggedClick) {
      justDraggedClick = false;
      return;
    }

    const rect = canvasEl.getBoundingClientRect();
    const x = normalize(event.clientX - rect.left, rect.width);
    const y = normalize(event.clientY - rect.top, rect.height);
    config.points = [
      ...config.points,
      {
        id: nextId++,
        x,
        y,
        color: "rgb(0,0,0)",
      },
    ];
  }

  function denormalize(value: number, scale: number) {
    return value * scale;
  }
  function normalize(value: number, scale: number) {
    return value / scale;
  }

  function canvasPointerDown(event: PointerEvent) {
    const rect = canvasEl.getBoundingClientRect();
    const x = event.clientX - rect.left;
    const y = event.clientY - rect.top;

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
      canvasEl.setPointerCapture(event.pointerId);
      event.preventDefault();
    }
  }

  function canvasPointerMove(event: PointerEvent) {
    if (!isDragging || dragIndex === null) return;

    const rect = canvasEl.getBoundingClientRect();
    const x = normalize(event.clientX - rect.left, rect.width);
    const y = normalize(event.clientY - rect.top, rect.height);

    const newX = x + dragOffsetX;
    const newY = y + dragOffsetY;

    hasDragged = true;

    config.points = config.points.map((p, i) =>
      i === dragIndex ? { ...p, x: newX, y: newY } : p
    );
  }

  function canvasPointerUp(event: PointerEvent) {
    if (isDragging) {
      isDragging = false;
      dragIndex = null;
      canvasEl.releasePointerCapture(event.pointerId);
      if (hasDragged) {
        justDraggedClick = true; // suppress the subsequent click
      }
    }
  }

  // --- buttons: reset / export / import ---

  function resetPoints() {
    config.points = [];
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
          id?: number;
          x: number;
          y: number;
        }[];
        config.points = parsed.map((p, idx) => ({
          id: p.id ?? idx + 1,
          x: p.x,
          y: p.y,
          color: "rgb(0,0,0)",
        }));
        nextId = config.points.reduce((max, p) => Math.max(max, p.id), 0) + 1;
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
      <div class="relative">
        <video
          bind:this={videoEl}
          class="w-full z-0 relative"
          autoplay
          crossorigin="anonymous"
          muted
          loop
          controls
          on:canplay={onCanPlay}
        >
          <!-- fallback default video if you want one -->
          <source src={videoUrl} type="video/mp4" />
          Your browser does not support the video tag.
        </video>

        <canvas
          class="absolute inset-0 z-10"
          bind:this={canvasEl}
          on:click={canvasClick}
          on:pointerdown={canvasPointerDown}
          on:pointermove={canvasPointerMove}
          on:pointerup={canvasPointerUp}
        />

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
        {#if !isPlaying}<button
            class="bg-neutral-700 rounded p-1 w-20"
            on:click={() => {
              videoEl.play();
            }}>Play</button
          >{:else}
          <button
            class="bg-neutral-700 rounded p-1 w-20"
            on:click={() => {
              videoEl.pause();
            }}>Pause</button
          >{/if}
        <div class="bg-neutral-700 rounded p-1 w-full flex items-center">
          <input
            type="range"
            class="w-full"
            value={currentTime}
            max={duration}
            on:input={(e) => {
              const val = Number((e.target as HTMLInputElement).value);
              videoEl.currentTime = val;
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
        on:change={onVideoFileChange}
      />
      <div class="border rounded p-1 text-center">Load video</div>
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
      <button class="border rounded p-1" type="button" on:click={resetPoints}
        >Reset points</button
      >
    </label>
    <label class="flex flex-col gap-1">
      <span class="font-semibold">Import/Export</span>
      <button class="border rounded p-1" type="button" on:click={exportPoints}
        >Export points</button
      >
      <label class="flex flex-col gap-1">
        <input
          class="hidden"
          type="file"
          accept="application/json"
          on:change={onImportFileChange}
        />
        <div class="border rounded p-1 text-center">Import points</div>
      </label>
      <FFmpegDemo
        videoURL={videoUrl}
        points={config.points}
        radius={config.pointRadius}
      />
    </label>
  </div>
  <p>{store.message}</p>
</div>

<div class="flex gap-6 bg-black py-10 justify-around px-10 grow items-center">
  {#each config.points as point (point.id)}
    <div
      class="grow rounded-full aspect-square max-h-62 max-w-62"
      style={`background-color: ${point.color}`}
    ></div>
  {/each}
</div>
