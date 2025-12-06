import cv2
import numpy as np

video_path = "input.mp4"
points_path = "points.json"
default_radius = 50

import json

with open(points_path, "r") as f:
    points = json.load(f)["points"]

# Ensure each point has a radius
for p in points:
    if "radius" not in p:
        p["radius"] = default_radius

cap = cv2.VideoCapture(video_path)
fps = cap.get(cv2.CAP_PROP_FPS)
frame_idx = 0

# Precompute offsets per radius
from collections import defaultdict

offsets_for_radius = defaultdict(list)
for p in points:
    r = p["radius"]
    if r in offsets_for_radius:
        continue
    offsets = []
    for dy in range(-r, r + 1):
        for dx in range(-r, r + 1):
            if dx * dx + dy * dy <= r * r:
                offsets.append((dx, dy))
    offsets_for_radius[r] = np.array(offsets, dtype=np.int32)

results = []  # list of (time, point_index, r,g,b)

while True:
    ret, frame = cap.read()
    if not ret:
        break

    h, w, _ = frame.shape
    t = frame_idx / fps

    # if frame_idx % 3 != 0:
    #     frame_idx += 1
    #     continue

    # Optionally downscale frame here with cv2.resize

    for i, p in enumerate(points):
        cx = int(p["x"] * (w - 1))
        cy = int(p["y"] * (h - 1))
        offsets = offsets_for_radius[p["radius"]]

        xs = cx + offsets[:, 0]
        ys = cy + offsets[:, 1]

        # clip to frame bounds
        xs = np.clip(xs, 0, w - 1)
        ys = np.clip(ys, 0, h - 1)

        colors = frame[ys, xs]  # shape (N, 3) BGR
        b, g, r = colors[:, 0], colors[:, 1], colors[:, 2]
        results.append(
            (
                frame_idx,
                i,
                int(r.mean()),
                int(g.mean()),
                int(b.mean()),
            )
        )

    frame_idx += 1
    print(frame_idx, end="\r")

cap.release()

# Save results to CSV
import csv

with open("samples.csv", "w", newline="") as f:
    writer = csv.writer(f)
    writer.writerow(["frame", "point_index", "r", "g", "b"])
    for row in results:
        writer.writerow(row)


# Save results to json, grouped by point (led-1, led-2, ...)
output = [[] for _ in range(len(points))]
for frame_idx, point_idx, r, g, b in results:
    output[point_idx].append(
        {
            "frame": frame_idx,
            "r": r,
            "g": g,
            "b": b,
        }
    )
with open("samples.json", "w") as f:
    json.dump(output, f, indent=2)

print("Done. Results saved to samples.csv and samples.json")
