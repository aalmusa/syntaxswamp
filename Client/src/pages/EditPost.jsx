import { useState, useEffect } from "react";
import { Link, useNavigate, useParams } from "react-router-dom";
import EditorPane from "../components/EditorPane";
import Preview from "../components/Preview";
import "../styles/CreatePost.css";
import "../styles/common.css";
import { useAuth } from "../context/AuthContext";

function EditPost() {
  const navigate = useNavigate();
  const { postId } = useParams();
  const { user } = useAuth();
  const [title, setTitle] = useState("");
  const [htmlCode, setHtmlCode] = useState("");
  const [cssCode, setCssCode] = useState("");
  const [jsCode, setJsCode] = useState("");
  const [isPrivate, setIsPrivate] = useState(false);
  const [isSaving, setIsSaving] = useState(false);
  const [saveStatus, setSaveStatus] = useState(null);
  const [loading, setLoading] = useState(true);
  const [isOwner, setIsOwner] = useState(false);

  useEffect(() => {
    const fetchPost = async () => {
      try {
        const response = await fetch(`http://localhost:18080/posts/${postId}`, {
          headers: user?.token ? { "Authorization": `Bearer ${user.token}` } : {}
        });
        
        if (!response.ok) throw new Error("Post not found");
        const data = await response.json();

        // Check if user can edit this post
        const isOwner = data.user_id === parseInt(user.userId);
        if (data.isPrivate && !isOwner) {
          throw new Error("You cannot edit this private post");
        }

        setTitle(data.title);
        setHtmlCode(data.html_code);
        setCssCode(data.css_code);
        setJsCode(data.js_code);
        setIsPrivate(data.isPrivate || false);
        setIsOwner(isOwner);
      } catch (error) {
        setSaveStatus({ success: false, message: error.message });
        navigate('/');
      } finally {
        setLoading(false);
      }
    };
    fetchPost();
  }, [postId, user.userId, user.token, navigate]);

  const handleSave = async () => {
    if (isSaving) return;
    setIsSaving(true);
    setSaveStatus(null);

    try {
      const response = await fetch(`http://localhost:18080/posts/${postId}`, {
        method: "PUT",
        headers: {
          "Content-Type": "application/json",
          Authorization: `Bearer ${user.token}`,
        },
        body: JSON.stringify({
          title,
          html_code: htmlCode,
          css_code: cssCode,
          js_code: jsCode,
          isPrivate,
        }),
      });

      if (response.ok) {
        setSaveStatus({
          success: true,
          message: "Post updated successfully",
        });
        setTimeout(() => {
          navigate(`/posts/${postId}`);
        }, 1500);
      } else {
        const error = await response.text();
        setSaveStatus({ success: false, message: `Error: ${error}` });
      }
    } catch (error) {
      setSaveStatus({
        success: false,
        message: `Network error: ${error.message}`,
      });
    } finally {
      setIsSaving(false);
    }
  };

  if (loading) {
    return <div className="loading-container">Loading post...</div>;
  }

  return (
    <div className="page-container">
      <div className="page-header">
        <div className="header-left">
          <Link to={`/posts/${postId}`} className="button secondary-button">
            Back to Post
          </Link>
          <input
            type="text"
            value={title}
            onChange={(e) => setTitle(e.target.value)}
            className="title-input"
            placeholder="Enter post title..."
          />
        </div>
        <div className="header-actions">
          {isOwner && (
            <div className="privacy-toggle">
              <input
                type="checkbox"
                id="privacy-toggle"
                checked={isPrivate}
                onChange={(e) => setIsPrivate(e.target.checked)}
                className="privacy-checkbox"
              />
              <label htmlFor="privacy-toggle" className="privacy-label">
                {isPrivate ? "Private Post" : "Public Post"}
              </label>
            </div>
          )}
          <button
            className="button primary-button"
            onClick={handleSave}
            disabled={isSaving}
          >
            {isSaving ? "Saving..." : "Update Post"}
          </button>
        </div>
      </div>

      {saveStatus && (
        <div
          className={`save-status ${saveStatus.success ? "success" : "error"}`}
        >
          {saveStatus.message}
        </div>
      )}

      <div className="editors-container">
        <EditorPane
          language="html"
          code={htmlCode}
          onChange={setHtmlCode}
          label="HTML"
        />
        <EditorPane
          language="css"
          code={cssCode}
          onChange={setCssCode}
          label="CSS"
        />
        <EditorPane
          language="javascript"
          code={jsCode}
          onChange={setJsCode}
          label="JS"
        />
      </div>

      <div className="preview-container">
        <h3 className="preview-header">Live Preview</h3>
        <Preview htmlCode={htmlCode} cssCode={cssCode} jsCode={jsCode} />
      </div>
    </div>
  );
}

export default EditPost;
