import { useEffect, useMemo, useState } from "react";
import { UploadArea } from "./components/UploadArea";
import { EndpointSelector, loadSavedEndpoint } from "./components/EndpointSelector";
import { Toast } from "./components/Toast";
import { useUpload } from "./hooks/useUpload";
import { JobCard } from "./components/JobCard";
import { JobConfigForm, DEFAULT_CONFIG } from "./components/JobConfig";
import type { JobConfig } from "./types";
import { useHealth } from "./hooks/useHealth";

const DEFAULT_ENDPOINT = "http://localhost:8345";

export default function App() {
  const [apiBase, setApiBase] = useState(() => loadSavedEndpoint(DEFAULT_ENDPOINT));
  const [selectedFile, setSelectedFile] = useState<File | null>(null);
  const [jobs, setJobs] = useState<string[]>([]);
  const [toast, setToast] = useState<string | null>(null);
  const [jobConfig, setJobConfig] = useState<JobConfig>(DEFAULT_CONFIG);

  const { upload, uploading, error: uploadError } = useUpload(apiBase);
  const health = useHealth(apiBase);

  useEffect(() => {
    if (uploadError) setToast(uploadError);
  }, [uploadError]);

  const canUpload = useMemo(() => !!selectedFile && !uploading, [selectedFile, uploading]);

  const handleUpload = async () => {
    if (!selectedFile) return;
    try {
      const result = await upload(selectedFile, jobConfig);
      setJobs((prev) => [result.id, ...prev]);
      setToast(null);
    } catch (err) {
      // error already captured in hook
      console.error(err);
    }
  };

  const reset = () => {
    setSelectedFile(null);
  };

  return (
    <div className="page">
      <header className="header">
        <div>
          <h1>Stemsmith</h1>
          <p className="muted">Upload a WAV, track progress, and download separated stems.</p>
        </div>
        <EndpointSelector value={apiBase} onChange={setApiBase} health={health} />
      </header>

      <main className="layout">
        <section className="panel">
          <UploadArea file={selectedFile} onFileChange={setSelectedFile} disabled={uploading} />
          <JobConfigForm value={jobConfig} onChange={setJobConfig} />
          <div className="actions">
            <button className="secondary" onClick={reset} disabled={!selectedFile || uploading}>
              Reset
            </button>
            <button className="primary" onClick={handleUpload} disabled={!canUpload}>
              {uploading ? "Uploading..." : "Upload"}
            </button>
          </div>
        </section>

        <section className="jobs-list">
          {jobs.length === 0 ? (
            <div className="panel">
              <p className="muted">No jobs yet. Upload a WAV to start.</p>
            </div>
          ) : (
            jobs.map((id) => <JobCard key={id} baseUrl={apiBase} jobId={id} onRemove={(jobId) => setJobs((prev) => prev.filter((j) => j !== jobId))} />)
          )}
        </section>
      </main>

      <Toast message={toast} kind="error" onClose={() => setToast(null)} />
    </div>
  );
}
