import '../styles/DeleteConfirmModal.css';

function DeleteConfirmModal({ isOpen, onClose, onConfirm, isDeleting }) {
  if (!isOpen) return null;

  return (
    <div className="modal-overlay">
      <div className="modal-content">
        <h2>Delete Post</h2>
        <p>Are you sure you want to delete this post? This action cannot be undone.</p>
        <div className="modal-actions">
          <button 
            className="button secondary-button" 
            onClick={onClose}
            disabled={isDeleting}
          >
            Cancel
          </button>
          <button 
            className="button danger-button" 
            onClick={onConfirm}
            disabled={isDeleting}
          >
            {isDeleting ? "Deleting..." : "Delete"}
          </button>
        </div>
      </div>
    </div>
  );
}

export default DeleteConfirmModal;
