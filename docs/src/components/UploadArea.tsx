import { useRef, useState } from "react";

interface Props {
  file: File | null;
  onFileChange: (file: File | null) => void;
  disabled?: boolean;
}

export function UploadArea({ file, onFileChange, disabled }: Props) {
  const inputRef = useRef<HTMLInputElement | null>(null);
  const [dragOver, setDragOver] = useState(false);

  const handleFiles = (files: FileList | null) => {
    if (!files || files.length === 0) return;
    const picked = files[0];
    if (!picked.name.toLowerCase().endsWith(".wav")) {
      alert("Please select a WAV file");
      return;
    }
    onFileChange(picked);
    if (inputRef.current) {
      inputRef.current.value = "";
    }
  };

  const onDrop = (e: React.DragEvent<HTMLDivElement>) => {
    e.preventDefault();
    e.stopPropagation();
    setDragOver(false);
    if (disabled) return;
    handleFiles(e.dataTransfer.files);
  };

  const onDragOver = (e: React.DragEvent<HTMLDivElement>) => {
    e.preventDefault();
    e.stopPropagation();
    if (disabled) return;
    setDragOver(true);
  };

  const onDragLeave = (e: React.DragEvent<HTMLDivElement>) => {
    e.preventDefault();
    e.stopPropagation();
    setDragOver(false);
  };

  const openPicker = () => {
    if (disabled) return;
    inputRef.current?.click();
  };

  return (
    <div className={`upload-area ${dragOver ? "drag-over" : ""} ${disabled ? "disabled" : ""}`}>
      <div
        className="drop-zone"
        onDrop={onDrop}
        onDragOver={onDragOver}
        onDragLeave={onDragLeave}
        onClick={openPicker}
      >
        <p className="title">Drop a WAV file here</p>
        <p className="subtitle">or click to choose a file</p>
        <input
          ref={inputRef}
          type="file"
          accept=".wav,audio/wav,audio/x-wav,audio/wave"
          onChange={(e) => handleFiles(e.target.files)}
          style={{ display: "none" }}
        />
      </div>
      {file && (
        <div className="file-preview">
          <div>
            <div className="file-name">{file.name}</div>
            <div className="file-meta">{(file.size / 1024 / 1024).toFixed(2)} MB</div>
          </div>
          <audio controls src={URL.createObjectURL(file)} />
        </div>
      )}
    </div>
  );
}
