import { useDownload } from "../hooks/useDownload";
import { useJobStatus } from "../hooks/useJobStatus";
import { StatusPanel } from "./StatusPanel";

interface Props {
  baseUrl: string;
  jobId: string;
}

export function JobCard({ baseUrl, jobId }: Props) {
  const { data, error, loading } = useJobStatus(baseUrl, jobId);
  const { download, downloading, error: downloadError } = useDownload(baseUrl);

  const handleDownload = async () => {
    await download(jobId);
  };

  return (
    <div className="panel">
      <StatusPanel
        status={data}
        loading={loading}
        error={error || downloadError}
        onDownload={handleDownload}
        downloading={downloading}
        jobId={jobId}
      />
    </div>
  );
}
