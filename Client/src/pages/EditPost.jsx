import { useState, useEffect } from "react";
import { Link, useNavigate, useParams } from 'react-router-dom';
import EditorPane from "../components/EditorPane";
import Preview from "../components/Preview";
import "../styles/CreatePost.css";
import '../styles/common.css';

function EditPost() {
  const navigate = useNavigate();
  const { postId } = useParams();
  const [title, setTitle] = useState("");
  const [htmlCode, setHtmlCode] = useState("");
  const [cssCode, setCssCode] = useState("");
  const [jsCode, setJsCode] = useState("");
  const [isSaving, setIsSaving] = useState(false);
  const [saveStatus, setSaveStatus] = useState(null);
  const [loading, setLoading] = useState(true);

  useEffect(() => {
    const fetchPost = async () => {
      try {
        const response = await fetch(`http://localhost:18080/posts/${postId}`);
        if (!response.ok) throw new Error('Post not found');
        const data = await response.json();
        setTitle(data.title);
        setHtmlCode(data.html_code);
        setCssCode(data.css_code);
        setJsCode(data.js_code);
      } catch (error) {
        setSaveStatus({ success: false, message: error.message });
      } finally {
        setLoading(false);
      }
    };
    fetchPost();
  }, [postId]);

  const handleSave = async () => {
    if (isSaving) return;
    setIsSaving(true);
    setSaveStatus(null);

    try {
      const response = await fetch(`http://localhost:18080/posts/${postId}`, {
        method: "PUT",
        headers: {
          "Content-Type": "application/json",
        },
        body: JSON.stringify({
          title,
          html_code: htmlCode,
          css_code: cssCode,
          js_code: jsCode,
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
          <Link to={`/posts/${postId}`} className="button secondary-button">Back to Post</Link>
          <input
            type="text"
            value={title}
            onChange={(e) => setTitle(e.target.value)}
            className="title-input"
            placeholder="Enter post title..."
          />
        </div>
        <button
          className="button primary-button"
          onClick={handleSave}
          disabled={isSaving}
        >
          {isSaving ? "Saving..." : "Update Post"}
        </button>
      </div>

      {saveStatus && (
        <div className={`save-status ${saveStatus.success ? "success" : "error"}`}>
          {saveStatus.message}
        </div>
      )}

      <div className="editors-container">
        <EditorPane language="html" code={htmlCode} onChange={setHtmlCode} label="HTML" />
        <EditorPane language="css" code={cssCode} onChange={setCssCode} label="CSS" />
        <EditorPane language="javascript" code={jsCode} onChange={setJsCode} label="JS" />
      </div>

      <div className="preview-container">
        <h3 className="preview-header">Live Preview</h3>
        <Preview htmlCode={htmlCode} cssCode={cssCode} jsCode={jsCode} />
      </div>
    </div>
  );
}

export default EditPost;
