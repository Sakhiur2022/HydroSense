"""
DUMMY DATA GENERATOR — for pipeline testing only.
Generates fake but plausibly-shaped depletion cycle CSVs so the ML notebooks
can be built and debugged before real STM32-logged data is available.

DELETE / STOP USING once real cycle data starts coming in.
"""

import numpy as np
import pandas as pd
from datetime import datetime, timedelta

np.random.seed(42)  # reproducible dummy data

OUTPUT_DIR = "/home/claude/ml/data/raw"

# Rough condition profiles: (temp_base, temp_amplitude, humidity_base,
# humidity_amplitude, light_base, light_amplitude, depletion_rate_pct_per_hr)
CONDITION_PROFILES = {
    "indoor": dict(temp_base=26, temp_amp=1.5, hum_base=55, hum_amp=5,
                   light_base=150, light_amp=100, depletion_rate=2.2),
    "outdoor": dict(temp_base=29, temp_amp=5.0, hum_base=65, hum_amp=15,
                     light_base=8000, light_amp=7000, depletion_rate=3.4),
}

START_MOISTURE = 90.0   # % VWC right after saturation watering
DRY_THRESHOLD = 25.0    # % VWC — cycle ends when moisture drops to this


def generate_cycle(condition, cycle_num, start_datetime):
    """Generate one depletion cycle's hourly log until moisture crosses threshold."""
    profile = CONDITION_PROFILES[condition]

    rows = []
    moisture = START_MOISTURE
    t = start_datetime
    hour_index = 0

    # cap cycle length so it can't run away forever in a weird random draw
    while moisture > DRY_THRESHOLD and hour_index < 80:
        hour_of_day = t.hour

        # diurnal light/temp pattern: peak around 1-3pm, near-zero light at night
        daylight_factor = max(0.0, np.sin((hour_of_day - 6) / 12 * np.pi))

        temperature = (profile["temp_base"]
                       + profile["temp_amp"] * daylight_factor
                       + np.random.normal(0, 0.6))

        humidity = (profile["hum_base"]
                    - profile["hum_amp"] * daylight_factor * 0.5
                    + np.random.normal(0, 2.5))
        humidity = float(np.clip(humidity, 10, 95))

        if condition == "outdoor":
            light = profile["light_base"] + profile["light_amp"] * daylight_factor
            light = max(0.0, light + np.random.normal(0, 300))
        else:
            # indoor: mostly artificial light, mild daytime bump, no true darkness swing
            light = profile["light_base"] + 40 * daylight_factor
            light = max(0.0, light + np.random.normal(0, 20))

        # depletion rate increases somewhat with temperature/light (physically plausible)
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
    import os
    os.makedirs(OUTPUT_DIR, exist_ok=True)

    cycle_start_time = datetime(2026, 7, 1, 9, 0, 0)

    manifest_rows = []

    for condition in ["indoor", "outdoor"]:
        for cycle_num in range(1, 3):  # 2 dummy cycles per condition
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

            print(f"Generated {filename}: {len(df)} hourly rows "
                  f"({df['soil_moisture_pct'].iloc[0]:.1f}% -> {df['soil_moisture_pct'].iloc[-1]:.1f}%)")

            # stagger next cycle's start a bit so timestamps look realistic
            cycle_start_time = pd.to_datetime(df["timestamp"].iloc[-1]) + timedelta(hours=3)

    manifest = pd.DataFrame(manifest_rows)
    manifest_path = os.path.join(OUTPUT_DIR, "..", "processed", "cycle_manifest_DUMMY.csv")
    os.makedirs(os.path.dirname(manifest_path), exist_ok=True)
    manifest.to_csv(manifest_path, index=False)
    print(f"\nManifest written to {manifest_path}")


if __name__ == "__main__":
    main()
