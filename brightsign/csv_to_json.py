import csv
import json
from pathlib import Path


def csv_to_json(csv_file_path: str, output_path: str = "samples.json") -> str:
    """
    Convert a CSV file to grouped JSON format.

    Input CSV rows (no header) like:
        led-0,700,32,33,35

    Output JSON like:
    {
      "led-0": [
        {"time": 700, "red": 32, "green": 33, "blue": 35},
        ...
      ],
      "led-1": [...]
    }
    """
    csv_file_path = Path(csv_file_path)

    result = {}

    with csv_file_path.open(newline="", encoding="utf-8") as f:
        reader = csv.reader(f)
        for row in reader:
            if not row:
                continue  # skip empty lines

            client_id, time_str, red_str, green_str, blue_str = row

            entry = {
                "time": int(time_str),
                "red": int(red_str),
                "green": int(green_str),
                "blue": int(blue_str),
            }

            # Group by client_id
            result.setdefault(client_id, []).append(entry)

    for sample in result.values():
        for i in range(len(sample) - 1, 0, -1):
            if (
                abs(sample[i]["red"] - sample[i - 1]["red"]) < 5
                and abs(sample[i]["green"] - sample[i - 1]["green"]) < 5
                and abs(sample[i]["blue"] - sample[i - 1]["blue"]) < 5
            ):
                del sample[i]

    for sample in result.values():
        for i in range(len(sample) - 1, 0, -1):
            sample[i]["time"] -= sample[i - 1]["time"]

    # Pretty JSON
    json_str = json.dumps(result, indent=2)

    # Write to file
    with open(output_path, "w", encoding="utf-8") as f:
        f.write(json_str)

    return json_str


def main():
    csv_file = "sequence.csv"  # Replace with your CSV file path
    csv_to_json(csv_file, "samples.json")
    print("Conversion complete. JSON output written to samples.json")


if __name__ == "__main__":
    main()


"""
                function simplifySequenceForLight(sequence, lightId, {
        colorEps = 0.01,   // ~2–3/255
        maxGapS = 0.2      // don't interpolate over more than 200ms without a point
    } = {}) {
        const samples = sequence
            .filter(s => s.pointIndex === lightId)
            .sort((a, b) => a.t - b.t);

        if (samples.length <= 2) return samples;

        const simplified = [];
        let last = samples[0];
        simplified.push(last);

        for (let i = 1; i < samples.length - 1; i++) {
            const s = samples[i];

            const dr = Math.abs(s.r - last.r);
            const dg = Math.abs(s.g - last.g);
            const db = Math.abs(s.b - last.b);
            const diff = Math.max(dr, dg, db);

            const dt = s.t - last.t;

            if (diff >= colorEps || dt >= maxGapS) {
                simplified.push(s);
                last = s;
            }
        }

        // always keep the last sample
        simplified.push(samples[samples.length - 1]);

        return simplified;
    }
"""
