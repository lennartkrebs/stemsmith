import { useCallback, useState } from "react";
import { createApiClient } from "../api/client";

export function useCancel(baseUrl: string) {
  const [cancelling, setCancelling] = useState(false);
  const [error, setError] = useState<string | null>(null);

  const cancel = useCallback(
    async (id: string) => {
      setCancelling(true);
      setError(null);
      try {
        const client = createApiClient({ baseUrl });
        await client.cancel(id);
      } catch (err) {
        const message = err instanceof Error ? err.message : "Cancel failed";
        setError(message);
        throw err;
      } finally {
        setCancelling(false);
      }
    },
    [baseUrl]
  );

  return { cancel, cancelling, error };
}
