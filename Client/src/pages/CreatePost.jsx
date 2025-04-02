import { useState } from "react";
import { Link, useNavigate } from "react-router-dom";
import { useAuth } from "../context/AuthContext";
import EditorPane from "../components/EditorPane";
import Preview from "../components/Preview";
import "../styles/CreatePost.css";
import "../styles/common.css";

function CreatePost() {
  const navigate = useNavigate();
  const { user } = useAuth();
  const [title, setTitle] = useState("Untitled Post");
  const [htmlCode, setHtmlCode] = useState("<!-- HTML code here -->");
  const [cssCode, setCssCode] = useState("/* CSS code here */");
  const [jsCode, setJsCode] = useState("// JavaScript code here");
  const [isPrivate, setIsPrivate] = useState(false);
  const [isSaving, setIsSaving] = useState(false);
  const [saveStatus, setSaveStatus] = useState(null);

  const handleSave = async () => {
    if (isSaving) return;

    setIsSaving(true);
    setSaveStatus(null);

    try {
      const response = await fetch("http://localhost:18080/posts", {
        method: "POST",
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
        const result = await response.json();
        setSaveStatus({
          success: true,
          message: `Post saved with ID: ${result.id}`,
        });

        // Navigate to post page after successful save
        setTimeout(() => {
          navigate(`/posts/${result.id}`);
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

  return (
    <div className="page-container">
      <div className="page-header">
        <div className="header-left">
          <Link to="/" className="button secondary-button">
            Back to Home
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
          <button
            className="button primary-button"
            onClick={handleSave}
            disabled={isSaving}
          >
            {isSaving ? "Saving..." : "Save Post"}
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

export default CreatePost;
