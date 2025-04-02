import { useState, useEffect } from "react";
import { useParams, Link, useNavigate } from "react-router-dom";
import { useAuth } from "../context/AuthContext";
import EditorPane from "../components/EditorPane";
import Preview from "../components/Preview";
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

  useEffect(() => {
    const fetchPost = async () => {
      try {
        setLoading(true);

        // Build headers with authentication token when available
        const headers = {};
        if (user && user.token) {
          headers["Authorization"] = `Bearer ${user.token}`;
          console.log("Sending request with auth token");
        } else {
          console.log("No authentication token available");
        }

        const response = await fetch(`http://localhost:18080/posts/${postId}`, {
          headers,
        });

        if (!response.ok) {
          const errorText = await response.text();
          if (response.status === 403) {
            throw new Error(`Access denied: ${errorText}`);
          } else {
            throw new Error(`Error ${response.status}: ${errorText}`);
          }
        }

        const data = await response.json();
        setPost(data);
        setError(null);
      } catch (err) {
        setError(err.message || "Failed to load post. Please try again later.");
        console.error("Error fetching post:", err);
      } finally {
        setLoading(false);
      }
    };

    fetchPost();
  }, [postId, user]);

  const handleDelete = async () => {
    if (
      !window.confirm(
        "Are you sure you want to delete this post? This action cannot be undone."
      )
    ) {
      return;
    }

    setIsDeleting(true);
    try {
      const response = await fetch(`http://localhost:18080/posts/${postId}`, {
        method: "DELETE",
        headers: {
          Authorization: `Bearer ${user.token}`,
        },
      });

      if (response.ok) {
        alert("Post deleted successfully");
        navigate("/");
      } else {
        const data = await response.text();
        throw new Error(data || "Failed to delete post");
      }
    } catch (err) {
      alert("Error deleting post: " + err.message);
    } finally {
      setIsDeleting(false);
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

  return (
    <div className="page-container">
      <div className="page-header">
        <div className="header-left">
          <h2 className="page-title">{post.title}</h2>
          {post.isPrivate && <span className="privacy-badge">Private</span>}
        </div>
        <div className="header-actions">
          <Link to={`/posts/${postId}/edit`} className="button primary-button">
            Edit Post
          </Link>
          <button
            onClick={handleDelete}
            className="button danger-button"
            disabled={isDeleting}
          >
            {isDeleting ? "Deleting..." : "Delete Post"}
          </button>
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
    </div>
  );
}

export default ViewPost;
