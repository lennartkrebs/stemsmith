import type { JobConfig, JobStatusResponse, UploadResult } from "../types";

export interface ApiClientOptions {
  baseUrl: string;
}

const jsonHeaders = {
  Accept: "application/json"
};

export function createApiClient({ baseUrl }: ApiClientOptions) {
  const url = (path: string) => `${baseUrl.replace(/\/$/, "")}${path}`;

  const upload = async (file: File, config?: JobConfig): Promise<UploadResult> => {
    const form = new FormData();
    // Force the part MIME to audio/wav for consistency.
    const wav = new File([file], file.name, { type: "audio/wav" });
    form.append("file", wav);
    
    if (config) {
      const cfg = JSON.stringify(config);
      form.append("config", cfg);
    }

    const resp = await fetch(url("/jobs"), {
      method: "POST",
      body: form
    });
    if (!resp.ok) {
      const text = await resp.text();
      throw new Error(`Upload failed (${resp.status}): ${text}`);
    }
    return resp.json();
  };

  const getStatus = async (id: string): Promise<JobStatusResponse> => {
    const resp = await fetch(url(`/jobs/${id}`), {
      method: "GET",
      headers: jsonHeaders
    });
    if (resp.status === 404) {
      throw new Error("Job not found");
    }
    if (!resp.ok) {
      throw new Error(`Status check failed (${resp.status})`);
    }
    return resp.json();
  };

  const download = async (id: string): Promise<Blob> => {
    const resp = await fetch(url(`/jobs/${id}/download`), {
      method: "GET"
    });
    if (!resp.ok) {
      const text = await resp.text();
      throw new Error(`Download failed (${resp.status}): ${text}`);
    }
    return resp.blob();
  };

  const cancel = async (id: string): Promise<void> => {
    const resp = await fetch(url(`/jobs/${id}`), {
      method: "DELETE"
    });
    if (!resp.ok && resp.status !== 404) {
      const text = await resp.text();
      throw new Error(`Cancel failed (${resp.status}): ${text}`);
    }
  };

  return { upload, getStatus, download, cancel };
}
