import {useCallback, useState} from "react";
import {createApiClient} from "../api/client";
import type {JobConfig, UploadResult} from "../types";

export function useUpload(baseUrl: string) {
  const [uploading, setUploading] = useState(false);
  const [error, setError] = useState<string | null>(null);

  const upload = useCallback(
    async (file: File, config?: JobConfig): Promise<UploadResult> => {
      setUploading(true);
      setError(null);
      try {
        const client = createApiClient({ baseUrl });
        return await client.upload(file, config);
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
