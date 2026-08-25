## 01_data_loading_and_exploration.ipynb

- Load all raw CSVs from data/raw/ (one per cycle)
- Basic sanity checks: correct columns present, no wildly out-of-range values (e.g., humidity > 100%, negative moisture)
- Plot each cycle's raw moisture-over-time curve — just eyeball that it actually looks like a depletion curve (starts high, trends down)
- Quick print of cycle count per condition (indoor vs outdoor) so far

## 02_data_cleaning_and_cycle_validation.ipynb

- Load/build cycle_manifest.csv — one row per cycle: condition, team member, start time, end time, notes
- Flag and exclude any cycle that got contaminated: rain got into a "sheltered" outdoor pot, keypad override accidentally triggered mid-cycle, sensor clearly malfunctioned (flat-lined reading, impossible jump)
- Document why each excluded cycle was excluded (you'll want this for your report's limitations/data section)
- Output: a clean list of valid cycles to proceed with

## 03_feature_engineering.ipynb

- Compute moisture trend (Δ over last 2–3 hourly readings) for every timestamp in every valid cycle
- Extract hour-of-day (0–23) from each timestamp
- Combine temperature, humidity, moisture, moisture trend, light, hour-of-day into one feature table per timestamp
- Sanity-check: no missing values, trend calculation looks right at cycle boundaries (first 2-3 hours of a cycle won't have a valid trend yet — decide how you handle that, e.g., drop those rows)

## 04_label_construction.ipynb

- For each valid cycle, find its actual end time (threshold-crossing timestamp)
- For every hourly row in that cycle, compute time-remaining = (cycle end time − this row's time)
- Map time-remaining to urgency bucket (<24h / 24–48h / >48h)
- Attach this label to each row from notebook 03
- Output: labeled_dataset.csv — full feature + label table, saved to data/processed/

## 05_exploratory_data_analysis.ipynb

- Class balance check: how many examples per urgency bucket, per condition — flag if any class is very sparse
- Feature distributions: does temperature/humidity/light actually look different between indoor and outdoor data, as expected?
- Correlation check between features (e.g., is moisture trend basically redundant with instantaneous moisture, or genuinely adding signal?)
- This is where you visually confirm your data looks reasonable before trusting any model trained on it

## 06_train_test_split_strategy.ipynb

- Implement leave-one-cycle-out splitting logic, separately for indoor and outdoor
- Explicitly verify no data leakage: confirm all rows from the same cycle always stay together (never split across train/test)
- This notebook's whole job is producing the train/test cycle groupings that notebooks 07–08 will loop through — nothing else

## 07_model_training.ipynb

- Train all three candidates — logistic regression, decision tree, random forest — using the splits from notebook 06
- Loop through each leave-one-cycle-out fold, training a fresh model each time
- Save trained models (or at least the final full-data versions) to models

## 08_model_evaluation.ipynb

- For each model, collect predictions across all leave-one-cycle-out folds
- Compute accuracy and confusion matrix, separately for indoor and outdoor
- Save result plots/tables to results/

## 09_model_comparison_and_selection.ipynb

- Side-by-side comparison table: all three models' accuracy, per condition
- Discussion/notes: which performed best, any surprises, does performance differ meaningfully between indoor/outdoor
- Explicit statement: regardless of this comparison, decision tree is selected for deployment (document why, referencing your methodology doc)

## 10_decision_tree_export_for_deployment.ipynb

- Print/export the final decision tree's structure (sklearn's export_text or similar) in full
- Manually walk through translating it into C if/else pseudocode, right there in the notebook, side-by-side with the sklearn output
- This becomes your decision_tree_c_reference.txt — the thing you copy from when writing the actual firmware code
