import type { JobStatusResponse } from "../types";

interface Props {
  status: JobStatusResponse | null;
  loading: boolean;
  error: string | null;
  onDownload?: () => void;
  downloading?: boolean;
  jobId?: string;
  onCancel?: () => void;
  cancelling?: boolean;
  onRemove?: (id: string) => void;
}

export function StatusPanel({
  status,
  loading,
  error,
  onDownload,
  downloading,
  jobId,
  onCancel,
  cancelling,
  onRemove
}: Props) {
  const progressValue = status?.progress ?? -1;
  const showProgress = status && status.status !== "unknown";
  const canCancel = status && (status.status === "queued" || status.status === "running");
  const statusError = status && status.status === "cancelled" ? null : status?.error;
  const isTerminal =
      status && (status.status === "completed" || status.status === "failed" || status.status === "cancelled");

  return (
    <div className="status-panel">
      <div className="status-header">
        <h3>Status</h3>
        <div className="status-actions">
          {(status?.id || jobId) && <span className="job-id">Job {status?.id ?? jobId}</span>}
          {onRemove && jobId && (
            <button
              className="link"
              onClick={() => {
                if (!isTerminal) {
                  // eslint-disable-next-line no-alert
                  const confirmed = window.confirm("Job is still running. Cancel it before removing?");
                  if (!confirmed) return;
                  if (onCancel) {
                    onCancel();
                    return;
                  }
                }
                onRemove(jobId);
              }}
            >
              ✕
            </button>
          )}
        </div>
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
          {statusError && <div className="error-text">{statusError}</div>}
          <div className="actions">
            <button className="secondary" disabled={!canCancel || cancelling} onClick={onCancel}>
              {cancelling ? "Cancelling..." : "Cancel"}
            </button>
            <button
              className="primary"
              disabled={!status || status.status !== "completed" || downloading}
              onClick={onDownload}
            >
              {downloading ? "Downloading..." : "Download stems"}
            </button>
          </div>
        </>
      ) : (
        <p className="muted">{loading ? "Loading..." : "No job yet"}</p>
      )}
      {error && <div className="error-text">{error}</div>}
    </div>
  );
}
