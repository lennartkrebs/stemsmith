import { useEffect, useState } from "react";

interface Props {
  value: string;
  onChange: (value: string) => void;
  health?: "unknown" | "ok" | "fail";
}

const STORAGE_KEY = "stemsmith-api-base";

export function EndpointSelector({ value, onChange, health = "unknown" }: Props) {
  const [input, setInput] = useState(value);

  useEffect(() => {
    setInput(value);
  }, [value]);

  const handleBlur = () => {
    const trimmed = input.trim().replace(/\/$/, "");
    setInput(trimmed);
    onChange(trimmed);
    try {
      localStorage.setItem(STORAGE_KEY, trimmed);
    } catch {
      // ignore
    }
  };

  return (
    <div className="endpoint">
      <label htmlFor="api-endpoint">API endpoint</label>
      <div className="endpoint-input">
        <input
          id="api-endpoint"
          type="text"
          value={input}
          onChange={(e) => setInput(e.target.value)}
          onBlur={handleBlur}
          placeholder="http://localhost:8345"
        />
        <span className={`health-dot health-${health}`} aria-label={`server ${health}`} />
      </div>
    </div>
  );
}

export function loadSavedEndpoint(defaultValue: string) {
  try {
    return localStorage.getItem(STORAGE_KEY) || defaultValue;
  } catch {
    return defaultValue;
  }
}
