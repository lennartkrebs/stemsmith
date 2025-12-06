import { useEffect, useRef, useState } from "react";
import { createApiClient } from "../api/client";
import type { JobStatusResponse, JobStatusValue } from "../types";

const TERMINAL: JobStatusValue[] = ["completed", "failed", "cancelled"];

export function useJobStatus(baseUrl: string, id: string | null, intervalMs = 1500) {
  const [data, setData] = useState<JobStatusResponse | null>(null);
  const [error, setError] = useState<string | null>(null);
  const [loading, setLoading] = useState(false);
  const timer = useRef<number | null>(null);

  useEffect(() => {
    if (!id) {
      setData(null);
      setError(null);
      return;
    }

    let cancelled = false;
    const client = createApiClient({ baseUrl });

    const poll = async () => {
      setLoading(true);
      try {
        const status = await client.getStatus(id);
        if (cancelled) return;
        setData(status);
        setError(null);
        if (!TERMINAL.includes(status.status)) {
          timer.current = window.setTimeout(poll, intervalMs);
        }
      } catch (err) {
        if (cancelled) return;
        const message = err instanceof Error ? err.message : "Status check failed";
        setError(message);
      } finally {
        setLoading(false);
      }
    };

    poll();

    return () => {
      cancelled = true;
      if (timer.current) {
        clearTimeout(timer.current);
      }
    };
  }, [baseUrl, id, intervalMs]);

  return { data, error, loading };
}
