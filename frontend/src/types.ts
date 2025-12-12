export type JobStatusValue = "queued" | "running" | "completed" | "failed" | "cancelled" | "unknown";

export interface JobStatusResponse {
  id: string;
  status: JobStatusValue;
  progress: number;
  output_dir?: string;
  error?: string;
}

export interface UploadResult {
  id: string;
}

export interface JobConfig {
  model: ModelProfileKey;
  stems: string[];
}

export type ModelProfileKey = "balanced-four-stem" | "balanced-six-stem";
