import type { JobStatusResponse } from "../types";

interface Props {
  status: JobStatusResponse | null;
  loading: boolean;
  error: string | null;
  onDownload?: () => void;
  downloading?: boolean;
  jobId?: string;
}

export function StatusPanel({ status, loading, error, onDownload, downloading, jobId }: Props) {
  const progressValue = status?.progress ?? -1;
  const showProgress = status && status.status !== "unknown";

  return (
    <div className="status-panel">
      <div className="status-header">
        <h3>Status</h3>
        {(status?.id || jobId) && <span className="job-id">Job {status?.id ?? jobId}</span>}
      </div>
      {showProgress ? (
        <>
          <div className="status-row">
            <span className="label">State:</span>
            <span className={`badge badge-${status?.status || "unknown"}`}>
              {status?.status ?? "unknown"}
            </span>
          </div>
          <div className="status-row">
            <span className="label">Progress:</span>
            <div className="progress">
              <div
                className="progress-bar"
                style={{
                  width: `${Math.max(0, Math.min(100, progressValue * 100))}%`
                }}
              />
            </div>
            <span className="progress-value">
              {progressValue < 0 ? "--" : `${Math.round(progressValue * 100)}%`}
            </span>
          </div>
          {status?.error && <div className="error-text">{status.error}</div>}
          <button
            className="primary"
            disabled={!status || status.status !== "completed" || downloading}
            onClick={onDownload}
          >
            {downloading ? "Downloading..." : "Download stems"}
          </button>
        </>
      ) : (
        <p className="muted">{loading ? "Loading..." : "No job yet"}</p>
      )}
      {error && <div className="error-text">{error}</div>}
    </div>
  );
}
