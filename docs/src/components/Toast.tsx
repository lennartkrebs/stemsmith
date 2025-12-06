interface Props {
  message: string | null;
  kind?: "error" | "info";
  onClose?: () => void;
}

export function Toast({ message, kind = "info", onClose }: Props) {
  if (!message) return null;
  return (
    <div className={`toast toast-${kind}`}>
      <span>{message}</span>
      {onClose && (
        <button className="link" onClick={onClose}>
          Close
        </button>
      )}
    </div>
  );
}
