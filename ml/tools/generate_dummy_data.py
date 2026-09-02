import numpy as np
import pandas as pd
from datetime import datetime, timedelta
import os

np.random.seed(42)
OUTPUT_DIR = "/home/claude/ml/data/raw"
PROCESSED_DIR = "/home/claude/ml/data/preprocessed"

CONDITION_PROFILES = {
    "indoor": dict(temp_base=26, temp_amp=1.5, hum_base=55, hum_amp=5,
                   light_base=150, light_amp=100, depletion_rate=0.85),
    "outdoor": dict(temp_base=29, temp_amp=5.0, hum_base=65, hum_amp=15,
                     light_base=8000, light_amp=7000, depletion_rate=0.95),
}
START_MOISTURE = 90.0
DRY_THRESHOLD = 25.0

def generate_cycle(condition, cycle_num, start_datetime):
    profile = CONDITION_PROFILES[condition]
    rows = []
    moisture = START_MOISTURE
    t = start_datetime
    hour_index = 0
    while moisture > DRY_THRESHOLD and hour_index < 110:
        hour_of_day = t.hour
        daylight_factor = max(0.0, np.sin((hour_of_day - 6) / 12 * np.pi))
        temperature = (profile["temp_base"] + profile["temp_amp"] * daylight_factor + np.random.normal(0, 0.6))
        humidity = (profile["hum_base"] - profile["hum_amp"] * daylight_factor * 0.5 + np.random.normal(0, 2.5))
        humidity = float(np.clip(humidity, 10, 95))
        if condition == "outdoor":
            light = profile["light_base"] + profile["light_amp"] * daylight_factor
            light = max(0.0, light + np.random.normal(0, 300))
        else:
            light = profile["light_base"] + 40 * daylight_factor
            light = max(0.0, light + np.random.normal(0, 20))
        rate = profile["depletion_rate"] * (1 + 0.15 * daylight_factor) + np.random.normal(0, 0.3)
        rate = max(0.2, rate)
        moisture = max(0.0, moisture - rate)
        rows.append({
            "timestamp": t.strftime("%Y-%m-%d %H:%M:%S"),
            "temperature_C": round(temperature, 2),
            "humidity_pct": round(humidity, 2),
            "soil_moisture_pct": round(moisture, 2),
            "light_lux": round(light, 1),
            "condition": condition,
            "cycle_id": f"{condition}_cycle{cycle_num:02d}",
        })
        t += timedelta(hours=1)
        hour_index += 1
    return pd.DataFrame(rows)

def main():
    os.makedirs(OUTPUT_DIR, exist_ok=True)
    os.makedirs(PROCESSED_DIR, exist_ok=True)
    cycle_start_time = datetime(2026, 7, 1, 9, 0, 0)
    manifest_rows = []
    for condition in ["indoor", "outdoor"]:
        for cycle_num in range(1, 6):
            df = generate_cycle(condition, cycle_num, cycle_start_time)
            filename = f"{condition}_cycle{cycle_num:02d}_DUMMY.csv"
            filepath = os.path.join(OUTPUT_DIR, filename)
            df.to_csv(filepath, index=False)
            manifest_rows.append({
                "cycle_id": f"{condition}_cycle{cycle_num:02d}",
                "condition": condition,
                "team_member": "DUMMY_GENERATOR",
                "start_time": df["timestamp"].iloc[0],
                "end_time": df["timestamp"].iloc[-1],
                "duration_hours": len(df),
                "excluded": False,
                "exclusion_reason": "",
                "notes": "Synthetic placeholder data for pipeline testing only.",
            })
            print(f"Generated {filename}: {len(df)} hourly rows")
            cycle_start_time = pd.to_datetime(df["timestamp"].iloc[-1]) + timedelta(hours=3)
    manifest = pd.DataFrame(manifest_rows)
    manifest_path = os.path.join(PROCESSED_DIR, "cycle_manifest_DUMMY.csv")
    manifest.to_csv(manifest_path, index=False)
    print(f"Manifest written to {manifest_path}")

if __name__ == "__main__":
    main()
