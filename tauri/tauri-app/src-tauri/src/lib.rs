// Learn more about Tauri commands at https://tauri.app/develop/calling-rust/
use opencv::{core, imgproc, prelude::*, videoio};
use serde::Deserialize;

#[derive(Deserialize, Debug)]
struct ExtractPoint {
    id: String,
    x: f32,
    y: f32,
    radius_norm: f32,
}

#[derive(Deserialize, Debug)]
struct ExtractRequest {
    video_path: String,
    fps: f32,
    points: Vec<ExtractPoint>,
}

#[tauri::command]
fn greet(name: &str) -> String {
    format!("Hello, {}! You've been greeted from Rust!", name)
}

#[tauri::command]
fn extract_sequences(config: ExtractRequest) -> Result<(), String> {
    println!("Received config from frontend: {:?}", config);
    let out_dir = "output"; // TODO: make configurable
    extract_with_opencv(config, &out_dir).map_err(|e| e.to_string())
    // TODO:
    // - Open video with OpenCV
    // - Sample colors for each point at `config.fps`
    // - Compress & write sequences (e.g. per-bulb binary or C header files)
    // For now we just pretend everything worked:
    // Ok(())
}

fn extract_with_opencv(config: ExtractRequest, out_dir: &str) -> opencv::Result<()> {
    use std::fs::File;
    use std::io::Write;

    let mut cap = videoio::VideoCapture::from_file(&config.video_path, videoio::CAP_ANY)?;
    if !cap.is_opened()? {
        return Err(opencv::Error::new(0, "Failed to open video".into()));
    }

    // Get video info
    let video_fps = cap.get(videoio::CAP_PROP_FPS)? as f32;
    let frame_count = cap.get(videoio::CAP_PROP_FRAME_COUNT)? as i32;

    // Step between frames to match target fps
    let step = (video_fps / config.fps).max(1.0);

    // Precompute radius offsets per radius
    use std::collections::HashMap;
    let mut radius_offsets: HashMap<i32, Vec<(i32, i32)>> = HashMap::new();
    for p in &config.points {
        radius_offsets.entry(p.radius).or_insert_with(|| {
            let mut offs = Vec::new();
            let r = p.radius;
            for dy in -r..=r {
                for dx in -r..=r {
                    if dx * dx + dy * dy <= r * r {
                        offs.push((dx, dy));
                    }
                }
            }
            offs
        });
    }

    // Prepare per-point output buffers
    let mut buffers: HashMap<String, Vec<(f32, u8, u8, u8)>> = HashMap::new();
    for p in &config.points {
        buffers.insert(p.id.clone(), Vec::new());
    }

    let mut frame = Mat::default();
    let mut frame_index = 0.0;

    while frame_index < frame_count as f32 {
        cap.set(videoio::CAP_PROP_POS_FRAMES, frame_index)?;
        if !cap.read(&mut frame)? || frame.empty()? {
            break;
        }

        let t_sec = frame_index / video_fps;

        // Optional: downscale here for speed
        // let mut small = Mat::default();
        // imgproc::resize(&frame, &mut small, core::Size::new(320, 0), 0.0, 0.0, imgproc::INTER_LINEAR)?;
        // let src = &small;

        let src = &frame;
        let height = src.rows();
        let width = src.cols();

        // Assume BGR 8-bit
        let mat = src; // alias

        for p in &config.points {
            let cx = (p.x * (width as f32 - 1.0)).round() as i32;
            let cy = (p.y * (height as f32 - 1.0)).round() as i32;
            let offsets = &radius_offsets[&p.radius];

            let mut r_sum: u64 = 0;
            let mut g_sum: u64 = 0;
            let mut b_sum: u64 = 0;
            let mut count: u64 = 0;

            for &(dx, dy) in offsets {
                let x = cx + dx;
                let y = cy + dy;
                if x < 0 || x >= width || y < 0 || y >= height {
                    continue;
                }

                let pix = mat.at_2d::<core::Vec3b>(y, x)?; // BGR
                let b = pix[0] as u64;
                let g = pix[1] as u64;
                let r = pix[2] as u64;

                b_sum += b;
                g_sum += g;
                r_sum += r;
                count += 1;
            }

            if count > 0 {
                let r = (r_sum / count) as u8;
                let g = (g_sum / count) as u8;
                let b = (b_sum / count) as u8;

                buffers.get_mut(&p.id).unwrap().push((t_sec, r, g, b));
            }
        }

        frame_index += step;
    }

    // TODO: compress & write sequences per bulb
    for (id, samples) in buffers {
        let path = format!("{}/{}.csv", out_dir, id); // or .bin / .h
        let mut file = File::create(path)?;
        for (t, r, g, b) in samples {
            writeln!(file, "{:.6},{},{},{}", t, r, g, b)?;
        }
    }

    Ok(())
}

#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub fn run() {
    tauri::Builder::default()
        .plugin(tauri_plugin_opener::init())
        .plugin(tauri_plugin_dialog::init())
        .invoke_handler(tauri::generate_handler![greet, extract_sequences])
        .run(tauri::generate_context!())
        .expect("error while running tauri application");
}
