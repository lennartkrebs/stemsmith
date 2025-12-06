import { useDownload } from "../hooks/useDownload";
import { useJobStatus } from "../hooks/useJobStatus";
import { useCancel } from "../hooks/useCancel";
import { StatusPanel } from "./StatusPanel";
import { useEffect, useState } from "react";

interface Props {
  baseUrl: string;
  jobId: string;
  onRemove?: (id: string) => void;
}

export function JobCard({ baseUrl, jobId, onRemove }: Props) {
  const { data, error, loading } = useJobStatus(baseUrl, jobId);
  const { download, downloading, error: downloadError } = useDownload(baseUrl);
  const { cancel, cancelling, error: cancelError } = useCancel(baseUrl);
  const [cancelRequested, setCancelRequested] = useState(false);

  const handleDownload = async () => {
    await download(jobId);
  };

  const handleCancel = async () => {
    setCancelRequested(true);
    try {
      await cancel(jobId);
    } catch (err) {
      setCancelRequested(false);
      throw err;
    }
  };

  useEffect(() => {
    if (data && data.status !== "queued" && data.status !== "running") {
      setCancelRequested(false);
    }
  }, [data]);

  return (
    <div className="panel job-card">
      <StatusPanel
        status={data}
        loading={loading}
        error={error || downloadError || cancelError}
        onDownload={handleDownload}
        downloading={downloading}
        jobId={jobId}
        onCancel={handleCancel}
        cancelling={cancelling || cancelRequested}
        onRemove={onRemove}
      />
    </div>
  );
}
