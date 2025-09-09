#!/usr/bin/env python3

from __future__ import annotations
import argparse, json, os, sys, csv, shutil, datetime as dt

# ======= Minimal TOML reader/writer for simple section/key=value configs =======

def _escape_string(s: str) -> str:
    return s.replace("\\", "\\\\").replace('"', '\\"')

def dump_toml(d: dict) -> str:
    lines = []
    scalars, tables = {}, {}
    for k, v in d.items():
        (tables if isinstance(v, dict) else scalars)[k] = v
    for k, v in scalars.items():
        lines.append(f"{k} = {format_toml_value(v)}")
    for tname in sorted(tables.keys()):
        lines.append(f"\n[{tname}]")
        for k, v in d[tname].items():
            lines.append(f"{k} = {format_toml_value(v)}")
    return ("\n".join(lines)).strip() + "\n"

def format_toml_value(v):
    if isinstance(v, bool):
        return "true" if v else "false"
    if isinstance(v, (int, float)):
        return str(v)
    if isinstance(v, str):
        return f"\"{_escape_string(v)}\""
    if isinstance(v, list):
        return "[" + ", ".join(format_toml_value(x) for x in v) + "]"
    return f"\"{_escape_string(str(v))}\""

def load_toml(path: str) -> dict:
    if not os.path.exists(path):
        raise FileNotFoundError(path)
    data, section = {}, None
    with open(path, "r", encoding="utf-8") as f:
        for raw in f:
            line = raw.strip()
            if not line or line.startswith("#"):
                continue
            if line.startswith("[") and line.endswith("]"):
                section = line[1:-1].strip()
                data.setdefault(section, {})
                continue
            if "=" in line:
                key, val = [x.strip() for x in line.split("=", 1)]
                target = data if section is None else data.setdefault(section, {})
                target[key] = parse_toml_scalar(val)
    return data

def parse_toml_scalar(s: str):
    if s.startswith('"') and s.endswith('"'):
        return bytes(s[1:-1], "utf-8").decode("unicode_escape")
    if s in ("true", "false"):
        return s == "true"
    try:
        if any(c in s for c in ".eE"):
            return float(s)
        return int(s)
    except ValueError:
        return s

# ======= Dotted-key helpers =======

def set_by_dotted_key(d: dict, dotted_key: str, value):
    parts = dotted_key.split(".")
    cur = d
    for p in parts[:-1]:
        if p not in cur or not isinstance(cur[p], dict):
            cur[p] = {}
        cur = cur[p]
    cur[parts[-1]] = value

def get_by_dotted_key(d: dict, dotted_key: str):
    parts, cur = dotted_key.split("."), d
    for p in parts:
        if not isinstance(cur, dict) or p not in cur:
            return None
        cur = cur[p]
    return cur

# ======= JSON + bookkeeping =======

def read_json(path: str, default):
    try:
        with open(path, "r", encoding="utf-8") as f:
            return json.load(f)
    except FileNotFoundError:
        return default

def write_json(path: str, data):
    with open(path, "w", encoding="utf-8") as f:
        json.dump(data, f, indent=2, sort_keys=True)

def append_manifest_row(manifest_path: str, row: dict, header):
    file_exists = os.path.exists(manifest_path)
    with open(manifest_path, "a", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fieldnames=header)
        if not file_exists:
            w.writeheader()
        w.writerow(row)

def timestamp() -> str:
    return dt.datetime.now().strftime("%Y%m%d_%H%M%S")

# ======= Main (always start from BASE, change exactly ONE var, write to OUTPUT) =======

def main(argv=None):
    p = argparse.ArgumentParser(
        description="Start from base.toml each run, change exactly ONE variation, update logging.folder for this experiment, and write to output TOML (no folders created)."
    )
    p.add_argument("--base", required=True, help="Path to base.toml (source of truth; never modified)")
    p.add_argument("--toml", required=True, help="Path to write the generated config.toml (overwritten each run)")
    p.add_argument("--vars", required=True, help="Path to variations.json (dot-keys -> list of values)")
    p.add_argument("--logs-root", default="logs", help="Prefix for logging.folder (default: logs)")
    p.add_argument("--state-name", default=None, help="State filename (default: alongside BASE as .<basename>.state.json)")
    p.add_argument("--manifest-name", default=None, help="Manifest filename (default: alongside BASE as <basename>.manifest.csv)")
    p.add_argument("--backup-output", action="store_true", help="If --toml exists, write a .bak before modifying")
    args = p.parse_args(argv)

    base_path = os.path.abspath(args.base)
    out_path  = os.path.abspath(args.toml)
    base_dir  = os.path.dirname(base_path)
    base_name = os.path.basename(base_path)

    state_path    = os.path.join(base_dir, args.state_name)    if args.state_name    else os.path.join(base_dir, f".{base_name}.state.json")
    manifest_path = os.path.join(base_dir, args.manifest_name) if args.manifest_name else os.path.join(base_dir, f"{base_name}.manifest.csv")

    # 1) Load BASE every run (we always start from it)
    base_cfg = load_toml(base_path)

    # 2) Load variations
    variations = read_json(args.vars, {})
    if not isinstance(variations, dict) or not variations:
        print(f"[ERROR] {args.vars} has no variations.", file=sys.stderr)
        sys.exit(2)
    keys = sorted(variations.keys())

    # 3) Load state (kept next to BASE so multiple outputs can share progress)
    state = read_json(state_path, {
        "run_idx": 0,
        "current_key_idx": 0,
        "per_key_value_idx": {k: 0 for k in keys},
        "keys": keys,
    })
    # Reconcile key set
    if state.get("keys") != keys:
        merged = {k: state.get("per_key_value_idx", {}).get(k, 0) for k in keys}
        state = {
            "run_idx": state.get("run_idx", 0),
            "current_key_idx": 0,
            "per_key_value_idx": merged,
            "keys": keys,
        }

    # 4) Pick ONE key/value to change this run
    cur_key = keys[state["current_key_idx"]]
    values = variations[cur_key]
    if not isinstance(values, list) or not values:
        print(f"[ERROR] variations for key '{cur_key}' must be a non-empty list.", file=sys.stderr)
        sys.exit(3)
    val_idx = state["per_key_value_idx"].get(cur_key, 0)
    value = values[val_idx]

    # 5) Build config FROM BASE, then modify ONLY that one key
    cfg = json.loads(json.dumps(base_cfg))  # cheap deep copy without importing deepcopy
    set_by_dotted_key(cfg, cur_key, value)

    # 6) Auto-set logging.folder to an experiment-tagged subpath (string only; no creation)
    # Example: logs/20250909_090000_run0007_settings_max_iter_2000
    exp_tag = f"{timestamp()}_run{state['run_idx']:04d}_{cur_key.replace('.', '_')}_{str(value).replace('/', '-')}"
    logs_folder = os.path.join(args.logs_root, exp_tag).replace("\\", "/")
    if "logging" not in cfg or not isinstance(cfg["logging"], dict):
        cfg["logging"] = {}
    cfg["logging"]["folder"] = logs_folder

    # 7) Backup output TOML if requested and exists
    if args.backup_output and os.path.exists(out_path):
        shutil.copy2(out_path, out_path + ".bak")

    # 8) Write OUTPUT TOML (overwriting)
    with open(out_path, "w", encoding="utf-8") as f:
        f.write(dump_toml(cfg))

    # 9) Advance state (next run returns to BASE and tweaks the next value/key)
    val_idx = (val_idx + 1) % len(values)
    state["per_key_value_idx"][cur_key] = val_idx
    if val_idx == 0:
        state["current_key_idx"] = (state["current_key_idx"] + 1) % len(keys)
    state["run_idx"] += 1
    write_json(state_path, state)

    # 10) Manifest (what was produced this run)
    row = {
        "output_toml": out_path,
        "var_key": cur_key,
        "var_value": value,
        "logging_folder": logs_folder,
        "written_at": dt.datetime.now().isoformat(timespec="seconds"),
        "run_idx": state["run_idx"],
    }
    append_manifest_row(manifest_path, row, list(row.keys()))

    print(f"[OK] Wrote {out_path} from base {base_path}")
    print(f"     Varied ONLY: {cur_key} = {value}")
    print(f"     logging.folder = {logs_folder}")
    print(f"     State:    {state_path}")
    print(f"     Manifest: {manifest_path}")

if __name__ == "__main__":
    main()
