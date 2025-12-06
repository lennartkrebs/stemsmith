import { useMemo, useState } from "react";
import type { JobConfig, ModelProfileKey } from "../types";

const PROFILE_STEMS: Record<ModelProfileKey, string[]> = {
  "balanced-four-stem": ["drums", "bass", "other", "vocals"],
  "balanced-six-stem": ["drums", "bass", "other", "vocals", "piano", "guitar"]
};

const STEM_LABEL: Record<string, string> = {
  drums: "Drums",
  bass: "Bass",
  other: "Other",
  vocals: "Vocals",
  piano: "Piano",
  guitar: "Guitar"
};

const MODEL_LABEL: Record<ModelProfileKey, string> = {
  "balanced-four-stem": "Balanced 4-Stem",
  "balanced-six-stem": "Balanced 6-Stem"
};

interface Props {
  value: JobConfig;
  onChange: (cfg: JobConfig) => void;
}

export function JobConfigForm({ value, onChange }: Props) {
  const [model, setModel] = useState<ModelProfileKey>(value.model);
  const [stems, setStems] = useState<string[]>(value.stems);
  const [modelOpen, setModelOpen] = useState(false);

  const availableStems = useMemo(() => PROFILE_STEMS[model], [model]);

  const toggleStem = (stem: string) => {
    setStems((prev) => {
      const next = prev.includes(stem) ? prev.filter((s) => s !== stem) : [...prev, stem];
      onChange({ model, stems: next });
      return next;
    });
  };

  const changeModel = (next: ModelProfileKey) => {
    setModel(next);
    const nextAvailable = PROFILE_STEMS[next];
    // prune stems not in the profile
    const filtered = stems.filter((s) => nextAvailable.includes(s));
    onChange({ model: next, stems: filtered });
    setStems(filtered);
    setModelOpen(false);
  };

  return (
    <div className="panel config-panel">
      <div className="config-row">
        <label htmlFor="model">Model profile</label>
        <div className="select-box">
          <button
            type="button"
            className="select-button"
            aria-haspopup="listbox"
            aria-expanded={modelOpen}
            onClick={() => setModelOpen((o) => !o)}
          >
            <span>{MODEL_LABEL[model]}</span>
            <span className="select-arrow">▾</span>
          </button>
          {modelOpen && (
            <ul className="select-options" role="listbox">
              {(Object.keys(MODEL_LABEL) as ModelProfileKey[]).map((key) => (
                <li key={key}>
                  <button
                    type="button"
                    className={`select-option ${model === key ? "selected" : ""}`}
                    onClick={() => changeModel(key)}
                  >
                    {MODEL_LABEL[key]}
                  </button>
                </li>
              ))}
            </ul>
          )}
        </div>
      </div>

      <div className="config-row">
        <div className="config-header">
          <label>Stems</label>
        </div>
        <div className="stem-grid">
          {availableStems.map((stem) => (
            <button
              type="button"
              key={stem}
              className={`chip ${stems.includes(stem) ? "chip-active" : ""}`}
              onClick={() => toggleStem(stem)}
              aria-pressed={stems.includes(stem)}
            >
              <span className="chip-indicator" aria-hidden="true" />
              {STEM_LABEL[stem] ?? stem}
            </button>
          ))}
        </div>
      </div>
    </div>
  );
}

export const DEFAULT_CONFIG: JobConfig = {
  model: "balanced-four-stem",
  stems: []
};
