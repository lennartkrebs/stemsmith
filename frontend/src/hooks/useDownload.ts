import { useCallback, useState } from "react";
import { createApiClient } from "../api/client";

export function useDownload(baseUrl: string) {
  const [downloading, setDownloading] = useState(false);
  const [error, setError] = useState<string | null>(null);

  const download = useCallback(
    async (id: string, filename = `job-${id}.zip`) => {
      setDownloading(true);
      setError(null);
      try {
        const client = createApiClient({ baseUrl });
        const blob = await client.download(id);
        const url = URL.createObjectURL(blob);
        const a = document.createElement("a");
        a.href = url;
        a.download = filename;
        document.body.appendChild(a);
        a.click();
        a.remove();
        URL.revokeObjectURL(url);
      } catch (err) {
        const message = err instanceof Error ? err.message : "Download failed";
        setError(message);
        throw err;
      } finally {
        setDownloading(false);
      }
    },
    [baseUrl]
  );

  return { download, downloading, error };
}
