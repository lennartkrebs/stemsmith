import { useEffect, useState } from "react";

export type HealthStatus = "unknown" | "ok" | "fail";

export function useHealth(baseUrl: string, intervalMs = 5000) {
  const [status, setStatus] = useState<HealthStatus>("unknown");

  useEffect(() => {
    let cancelled = false;
    let timer: number | undefined;

    const check = async () => {
      try {
        const resp = await fetch(`${baseUrl.replace(/\/$/, "")}/health`, { method: "GET" });
        if (cancelled) return;
        setStatus(resp.ok ? "ok" : "fail");
      } catch {
        if (cancelled) return;
        setStatus("fail");
      } finally {
        if (!cancelled) {
          timer = window.setTimeout(check, intervalMs);
        }
      }
    };

    setStatus("unknown");
    check();

    return () => {
      cancelled = true;
      if (timer) {
        clearTimeout(timer);
      }
    };
  }, [baseUrl, intervalMs]);

  return status;
}
