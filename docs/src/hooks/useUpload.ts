import { useState, useCallback } from "react";
import { createApiClient } from "../api/client";
import type { UploadResult } from "../types";

export function useUpload(baseUrl: string) {
  const [uploading, setUploading] = useState(false);
  const [error, setError] = useState<string | null>(null);

  const upload = useCallback(
    async (file: File): Promise<UploadResult> => {
      setUploading(true);
      setError(null);
      try {
        const client = createApiClient({ baseUrl });
        const result = await client.upload(file);
        return result;
      } catch (err) {
        const message = err instanceof Error ? err.message : "Upload failed";
        setError(message);
        throw err;
      } finally {
        setUploading(false);
      }
    },
    [baseUrl]
  );

  return { upload, uploading, error };
}
