import { useState, useEffect } from "react";
import { useParams, Link, useNavigate } from "react-router-dom";
import { useAuth } from "../context/AuthContext";
import EditorPane from "../components/EditorPane";
import Preview from "../components/Preview";
import DeleteConfirmModal from "../components/DeleteConfirmModal";
import "../styles/ViewPost.css";
import "../styles/common.css";

function ViewPost() {
  const { postId } = useParams();
  const navigate = useNavigate();
  const { user } = useAuth();
  const [post, setPost] = useState(null);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState(null);
  const [isDeleting, setIsDeleting] = useState(false);
  const [creator, setCreator] = useState(null);
  const [showDeleteModal, setShowDeleteModal] = useState(false);

  useEffect(() => {
    const fetchPostData = async () => {
      try {
        setLoading(true);
        // Fetch post and creator data in parallel
        const [postResponse, creatorResponse] = await Promise.all([
          fetch(`http://localhost:18080/posts/${postId}`, {
            headers: user?.token ? { "Authorization": `Bearer ${user.token}` } : {}
          }),
          fetch(`http://localhost:18080/posts/${postId}/creator`)
        ]);

        if (!postResponse.ok) {
          const errorText = await postResponse.text();
          if (postResponse.status === 403) {
            throw new Error(`Access denied: ${errorText}`);
          } else {
            throw new Error(`Error ${postResponse.status}: ${errorText}`);
          }
        }

        const postData = await postResponse.json();
        if (creatorResponse.ok) {
          const creatorData = await creatorResponse.json();
          setCreator(creatorData);
        }
        
        setPost(postData);
        setError(null);
      } catch (err) {
        setError(err.message || "Failed to load post. Please try again later.");
        console.error("Error fetching post:", err);
      } finally {
        setLoading(false);
      }
    };

    fetchPostData();
  }, [postId, user]);

  const handleDelete = async () => {
    setIsDeleting(true);
    try {
      if (!user?.token) {
        throw new Error('You must be logged in to delete posts');
      }

      const response = await fetch(`http://localhost:18080/posts/${postId}`, {
        method: 'DELETE',
        headers: {
          'Authorization': `Bearer ${user.token}`
        }
      });

      if (!response.ok) {
        const errorData = await response.text();
        throw new Error(errorData || 'Failed to delete post');
      }

      navigate('/');
    } catch (err) {
      console.error('Delete error:', err);
      alert(err.message);
    } finally {
      setIsDeleting(false);
      setShowDeleteModal(false);
    }
  };

  if (loading) {
    return <div className="loading-container">Loading post...</div>;
  }

  if (error || !post) {
    return (
      <div className="error-container">
        <h2>Error</h2>
        <p>{error || "Post not found"}</p>
      </div>
    );
  }

  const isOwner = user && post && parseInt(user.userId) === post.user_id;

  return (
    <div className="page-container">
      <div className="page-header">
        <div className="header-left">
          <div className="title-section">
            <h2 className="page-title">{post.title}</h2>
            {creator && (
              <div className="post-meta">
                Created by <span className="creator-name">{creator.username}</span>
                {post.isPrivate && <span className="privacy-badge">Private</span>}
              </div>
            )}
          </div>
        </div>
        <div className="header-actions">
          {isOwner && (
            <>
              <Link to={`/posts/${postId}/edit`} className="button primary-button">
                Edit Post
              </Link>
              <button
                onClick={() => setShowDeleteModal(true)}
                className="button danger-button"
              >
                Delete Post
              </button>
            </>
          )}
        </div>
      </div>

      <div className="post-content">
        <div className="editors-container">
          <EditorPane
            language="html"
            code={post.html_code}
            onChange={() => {}} // Read-only in view mode
            label="HTML"
          />
          <EditorPane
            language="css"
            code={post.css_code}
            onChange={() => {}} // Read-only in view mode
            label="CSS"
          />
          <EditorPane
            language="javascript"
            code={post.js_code}
            onChange={() => {}} // Read-only in view mode
            label="JS"
          />
        </div>

        <div className="preview-container">
          <h3 className="preview-header">Preview</h3>
          <Preview
            htmlCode={post.html_code}
            cssCode={post.css_code}
            jsCode={post.js_code}
          />
        </div>
      </div>

      <DeleteConfirmModal 
        isOpen={showDeleteModal}
        onClose={() => setShowDeleteModal(false)}
        onConfirm={handleDelete}
        isDeleting={isDeleting}
      />
    </div>
  );
}

export default ViewPost;
