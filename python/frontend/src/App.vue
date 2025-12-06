<template>
  <div class="grid lg:grid-cols-[3fr_3fr] gap-5 p-4">
    <!-- Left side: video + tools + timeline -->
    <div class="flex flex-col">
      <!-- Tool selector -->
      <div class="mb-2">
        <label class="flex items-center gap-2 mb-2">
          <span class="font-semibold text-neutral-200">Tool:</span>
          <select
            v-model="currentTool"
            class="bg-neutral-700 text-neutral-300 p-1 rounded"
          >
            <option
              v-for="[key, tool] in Object.entries(tools)"
              :key="key"
              :value="key"
            >
              {{ tool.label }}
            </option>
          </select>
        </label>
      </div>

      <!-- Video + point overlay -->
      <div class="relative overflow-clip">
        <video
          ref="videoEl"
          class="w-full z-10 relative"
          autoplay
          muted
          loop
          disablePictureInPicture
          crossorigin="anonymous"
          :class="tools[currentTool].cursor"
          v-if="videoUrl"
          @canplay="onCanPlay"
          @click="canvasClick"
          @pointerdown="canvasPointerDown"
          @pointermove="canvasPointerMove"
          @pointerup="canvasPointerUp"
          @mouseleave="canvasMouseLeave"
          @play="() => (isPlaying = true)"
          @pause="() => (isPlaying = false)"
          @timeupdate="onTimeUpdate"
        >
          <source :src="videoUrl" type="video/mp4" />
          Your browser does not support the video tag.
        </video>

        <!-- Overlay when no video loaded -->
        <div
          v-else
          class="aspect-video w-full flex items-center justify-center bg-neutral-900 text-neutral-500"
        >
          No video loaded
        </div>

        <!-- Draggable point markers -->
        <div
          v-for="(point, idx) in config.points"
          :key="idx"
          @pointerdown="pointPointerDown($event, idx)"
          class="absolute z-20 bg-blue-500/60 text-white font-mono px-1 select-none size-10 rounded-full flex items-center justify-center"
          :class="tools[currentTool].pointCursor"
          :style="{
            left: `${point.x * 100}%`,
            top: `${point.y * 100}%`,
            transform: `translate(-50%, -50%) scale(${
              config.pointRadius * 40
            })`,
          }"
        >
          {{ idx + 1 }}
        </div>
      </div>

      <!-- Playback controls -->
      <div
        v-if="videoEl"
        id="controls"
        class="flex items-center gap-2 mt-2 grow"
      >
        <button
          class="w-20 bg-neutral-700 hover:bg-neutral-600 text-neutral-100 rounded px-3 py-1 text-sm"
          @click="togglePlay"
        >
          {{ isPlaying ? "Pause" : "Play" }}
        </button>

        <div class="bg-neutral-700 rounded p-1 w-full flex items-center">
          <input
            type="range"
            class="w-full"
            :min="0"
            :max="duration"
            step="0.01"
            :value="currentTime"
            @input="onSeek"
          />
        </div>

        <div class="text-xs text-neutral-400 ml-2">
          {{ currentTime.toFixed(2) }} / {{ duration.toFixed(2) }} s
        </div>
      </div>
    </div>

    <!-- Right side: controls panel -->
    <div
      class="bg-neutral-800 p-4 rounded-lg flex flex-col gap-4 text-neutral-300"
    >
      <!-- Video selection -->
      <div class="flex flex-col gap-1">
        <span class="font-semibold">Video</span>
        <button
          class="bg-neutral-700 hover:bg-neutral-600 text-neutral-100 rounded px-3 py-1 text-sm text-left"
          @click="selectVideo"
        >
          {{ videoPath ? videoPath : "Select video file…" }}
        </button>
      </div>

      <!-- Radius -->
      <label class="flex flex-col gap-1">
        <span class="font-semibold">
          Radius: {{ (config.pointRadius * 100).toFixed(1) }} % width
        </span>
        <input
          type="range"
          min="0.001"
          max="0.1"
          step="0.001"
          v-model.number="config.pointRadius"
          @change="saveConfig"
        />
      </label>

      <!-- Reset -->
      <label class="flex flex-col gap-1">
        <span class="font-semibold">Reset</span>
        <button
          class="bg-neutral-700 hover:bg-neutral-600 text-neutral-100 rounded px-3 py-1 text-sm text-left"
          @click="resetConfig"
        >
          Reset config
        </button>
      </label>

      <!-- Import / Export points -->
      <div class="flex flex-col gap-2">
        <span class="font-semibold">Import/Export</span>
        <button
          class="bg-neutral-700 hover:bg-neutral-600 text-neutral-100 rounded px-3 py-1 text-sm text-left"
          @click="exportPoints"
        >
          Export points
        </button>

        <label class="flex flex-col gap-1">
          <input
            ref="importInputEl"
            class="hidden"
            type="file"
            accept="application/json"
            @change="onImportFileChange"
          />
          <button
            class="bg-neutral-700 hover:bg-neutral-600 text-neutral-100 rounded px-3 py-1 text-sm text-left"
            @click="triggerImport"
          >
            Import points
          </button>
        </label>
      </div>

      <!-- Rust-based extraction -->
      <div class="flex flex-col gap-2">
        <span class="font-semibold">Generate Sequences (Rust)</span>

        <label class="flex items-center gap-2 text-sm">
          <span class="whitespace-nowrap">Sample FPS:</span>
          <input
            type="number"
            min="1"
            max="120"
            v-model.number="targetFps"
            class="bg-neutral-700 text-neutral-100 rounded px-2 py-1 w-20"
          />
        </label>

        <button
          class="bg-emerald-600 hover:bg-emerald-500 text-white rounded px-3 py-1 text-sm text-left disabled:bg-neutral-600 disabled:cursor-not-allowed"
          :disabled="!videoPath || isExtracting"
          @click="runRustExtraction"
        >
          {{ isExtracting ? "Generating…" : "Generate sequences in Rust" }}
        </button>

        <div v-if="statusMessage" class="text-xs text-neutral-400">
          {{ statusMessage }}
        </div>
      </div>
    </div>
  </div>
</template>

<script setup lang="ts">
import { ref, reactive, onMounted } from "vue";

interface Color {
  r: number;
  g: number;
  b: number;
}

interface UiPoint {
  x: number; // normalized 0..1
  y: number; // normalized 0..1
  color: Color;
}

interface UiConfig {
  pointRadius: number; // normalized to video width
  points: UiPoint[];
}

interface ExtractPoint {
  id: string;
  x: number;
  y: number;
  radius_norm: number; // same normalized radius as pointRadius
}

interface ExtractRequest {
  video_path: string;
  fps: number;
  points: ExtractPoint[];
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
} as const;

type ToolKey = keyof typeof tools;

const STORAGE_KEY = "videoPointsConfig";

// refs
const videoEl = ref<HTMLVideoElement | null>(null);
const importInputEl = ref<HTMLInputElement | null>(null);

const currentTool = ref<ToolKey>("pointer");
const videoPath = ref<string | null>(null);
const videoUrl = ref<string | null>(null);

const isPlaying = ref(false);
const duration = ref(0);
const currentTime = ref(0);

// drag state
const isDragging = ref(false);
const dragIndex = ref<number | null>(null);
const dragOffsetX = ref(0);
const dragOffsetY = ref(0);

// extraction state
const targetFps = ref(25);
const isExtracting = ref(false);
const statusMessage = ref<string | null>(null);

// default config similar to your original
const defaultConfig: UiConfig = {
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
  ],
};

// nudge the default points into a diagonal like in your original
defaultConfig.points.forEach((p, index) => {
  p.x = index * (1 / defaultConfig.points.length) + 0.05;
  p.y = index * (1 / defaultConfig.points.length) + 0.05;
});

// reactive config
const config = reactive<UiConfig>(structuredClone(defaultConfig));

// --- persistence ---
function saveConfig() {
  try {
    localStorage.setItem(STORAGE_KEY, JSON.stringify(config));
  } catch (e) {
    console.error("Failed to save config", e);
  }
}

onMounted(() => {
  // load config from localStorage
  try {
    const stored = localStorage.getItem(STORAGE_KEY);
    if (stored) {
      const parsed = JSON.parse(stored) as UiConfig;
      config.pointRadius = parsed.pointRadius ?? defaultConfig.pointRadius;
      config.points.splice(
        0,
        config.points.length,
        ...parsed.points.map((p) => ({
          x: p.x,
          y: p.y,
          color: { r: 0, g: 0, b: 0 }, // color is not used anymore
        }))
      );
    }
  } catch (e) {
    console.error("Failed to parse stored config", e);
  }
});

// --- helpers ---
function normalize(value: number, scale: number) {
  return value / scale;
}
function denormalize(value: number, scale: number) {
  return value * scale;
}

// --- video control ---
function togglePlay() {
  if (!videoEl.value) return;
  if (isPlaying.value) {
    videoEl.value.pause();
  } else {
    videoEl.value.play();
  }
}

function onSeek(event: Event) {
  if (!videoEl.value) return;
  const target = event.target as HTMLInputElement;
  const val = Number(target.value);
  videoEl.value.currentTime = val;
  currentTime.value = val;
}

function onCanPlay(event: Event) {
  const v = event.target as HTMLVideoElement;
  duration.value = v.duration ?? 0;
}

function onTimeUpdate(event: Event) {
  const v = event.target as HTMLVideoElement;
  currentTime.value = v.currentTime;
}

// --- open html file dialog ---
async function selectVideo() {
  try {
    // Using pywebview to open file dialog
    const path: string = await (window as any).pywebview.api.open_file_dialog();
    if (path) {
      console.log("Selected video file:", path);
      videoPath.value = path;
      // Create object URL for video playback
      const response = await fetch(`file://${path}`);
      const blob = await response.blob();
      videoUrl.value = URL.createObjectURL(blob);
    }
  } catch (e) {
    console.error("Failed to select video file", e);
  }
}

// --- point editing ---
function canvasClick(event: MouseEvent) {
  event.preventDefault();
  event.stopPropagation();

  if (currentTool.value === "add") {
    addPointAtEvent(event);
  }
  // pointer/remove are handled via pointerdown on points/video
}

function addPointAtEvent(event: MouseEvent) {
  const video = videoEl.value;
  if (!video) return;
  const rect = video.getBoundingClientRect();
  const x = normalize(event.clientX - rect.left, rect.width);
  const y = normalize(event.clientY - rect.top, rect.height);
  config.points.push({
    x,
    y,
    color: { r: 0, g: 0, b: 0 },
  });
  saveConfig();
}

function pointPointerDown(event: PointerEvent, index: number) {
  event.stopPropagation();
  const video = videoEl.value;
  if (!video) return;

  if (currentTool.value === "add") return;

  if (currentTool.value === "remove") {
    config.points.splice(index, 1);
    saveConfig();
    return;
  }

  // pointer tool: start dragging this point
  dragIndex.value = index;
  isDragging.value = true;

  const rect = video.getBoundingClientRect();
  const x = event.clientX - rect.left;
  const y = event.clientY - rect.top;

  dragOffsetX.value = config.points[index]!.x - normalize(x, rect.width);
  dragOffsetY.value = config.points[index]!.y - normalize(y, rect.height);

  video.setPointerCapture(event.pointerId);
  event.preventDefault();
}

function canvasPointerDown(event: PointerEvent) {
  const video = videoEl.value;
  if (!video) return;

  if (currentTool.value !== "pointer") return;

  const rect = video.getBoundingClientRect();
  const x = event.clientX - rect.left;
  const y = event.clientY - rect.top;

  let foundIndex: number | null = null;
  for (let i = 0; i < config.points.length; i++) {
    const p = config.points[i]!;
    const dx = denormalize(p.x, rect.width) - x;
    const dy = denormalize(p.y, rect.height) - y;
    const radiusPx = denormalize(config.pointRadius, rect.width);
    if (dx * dx + dy * dy <= radiusPx * radiusPx) {
      foundIndex = i;
      break;
    }
  }

  if (foundIndex !== null) {
    dragIndex.value = foundIndex;
    isDragging.value = true;
    const idx = foundIndex;
    dragOffsetX.value = config.points[idx]!.x - normalize(x, rect.width);
    dragOffsetY.value = config.points[idx]!.y - normalize(y, rect.height);
    video.setPointerCapture(event.pointerId);
    event.preventDefault();
  }
}

function canvasPointerMove(event: PointerEvent) {
  if (!isDragging.value || dragIndex.value === null) return;
  const video = videoEl.value;
  if (!video) return;

  const rect = video.getBoundingClientRect();
  let x = normalize(event.clientX - rect.left, rect.width);
  let y = normalize(event.clientY - rect.top, rect.height);
  x = Math.max(0, Math.min(1, x));
  y = Math.max(0, Math.min(1, y));

  const newX = x + dragOffsetX.value;
  const newY = y + dragOffsetY.value;

  const idx = dragIndex.value;
  config.points = config.points.map((p, i) =>
    i === idx ? { ...p, x: newX, y: newY } : p
  ) as UiPoint[];
}

function canvasPointerUp(event: PointerEvent) {
  if (!isDragging.value) return;
  const video = videoEl.value;
  if (!video) return;

  config.points = config.points.map((p) => ({
    ...p,
    x: Math.max(0, Math.min(1, p.x)),
    y: Math.max(0, Math.min(1, p.y)),
  })) as UiPoint[];

  isDragging.value = false;
  dragIndex.value = null;
  video.releasePointerCapture(event.pointerId);
  saveConfig();
}

function canvasMouseLeave() {
  if (isDragging.value) {
    isDragging.value = false;
    dragIndex.value = null;
    saveConfig();
  }
}

// --- reset / import / export ---
function resetConfig() {
  config.pointRadius = defaultConfig.pointRadius;
  config.points.splice(
    0,
    config.points.length,
    ...structuredClone(defaultConfig.points)
  );
  localStorage.removeItem(STORAGE_KEY);
  saveConfig();
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

function triggerImport() {
  importInputEl.value?.click();
}

function onImportFileChange(event: Event) {
  const input = event.target as HTMLInputElement;
  const file = input.files?.[0];
  if (!file) return;

  const reader = new FileReader();
  reader.onload = (e) => {
    try {
      const text = String(e.target?.result);
      const parsed = JSON.parse(text) as {
        points: { x: number; y: number }[];
        pointRadius?: number;
      };
      config.pointRadius = parsed.pointRadius ?? config.pointRadius;
      config.points.splice(
        0,
        config.points.length,
        ...parsed.points.map((p) => ({
          x: p.x,
          y: p.y,
          color: { r: 0, g: 0, b: 0 },
        }))
      );
      saveConfig();
    } catch (err) {
      console.error("Failed to import points JSON", err);
      alert("Failed to import points JSON.");
    }
  };
  reader.readAsText(file);

  // allow re-importing same file
  input.value = "";
}

// --- Rust extraction ---
async function runRustExtraction() {
  if (!videoPath.value) {
    statusMessage.value = "Please select a video file first.";
    return;
  }
  if (config.points.length === 0) {
    statusMessage.value = "Please add at least one point.";
    return;
  }

  const req: ExtractRequest = {
    video_path: videoPath.value,
    fps: targetFps.value,
    points: config.points.map((p, idx) => ({
      id: `led-${idx + 1}`,
      x: p.x,
      y: p.y,
      radius_norm: config.pointRadius,
    })),
  };

  try {
    isExtracting.value = true;
    statusMessage.value = "Running extraction in Rust…";

    // call pywebview command
    console.log("Sending extraction request", req);
    await (window as any).pywebview.api.extract_sequences(req);

    statusMessage.value = "Extraction completed successfully.";
  } catch (e: any) {
    console.error("Extraction failed", e);
    statusMessage.value = `Extraction failed: ${String(e)}`;
  } finally {
    isExtracting.value = false;
  }
}
</script>

<style>
@import "tailwindcss";

html,
body {
  @apply bg-black;
}
</style>
