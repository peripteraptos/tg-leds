// Learn more about Tauri commands at https://tauri.app/develop/calling-rust/
#[tauri::command]
fn greet(name: &str) -> String {
    format!("Hello, {}! You've been greeted from Rust!", name)
}

#[tauri::command]
fn extract_sequences_dummy(config: String) -> String {
    println!("Got config from frontend: {}", config);
    // TODO: later parse JSON, run OpenCV, write files...
    "ok".to_string()
}


#[cfg_attr(mobile, tauri::mobile_entry_point)]
pub fn run() {
    tauri::Builder::default()
        .plugin(tauri_plugin_opener::init())
        .invoke_handler(tauri::generate_handler![greet, extract_sequences_dummy])
        
        .run(tauri::generate_context!())
        .expect("error while running tauri application");
}
