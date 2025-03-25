import { useState } from "react";
import { Link, useNavigate } from 'react-router-dom';
import EditorPane from "../components/EditorPane";
import Preview from "../components/Preview";
import "../styles/CreatePost.css";

function CreatePost() {
  const navigate = useNavigate();
  const [title, setTitle] = useState("Untitled Post");
  const [htmlCode, setHtmlCode] = useState("<!-- HTML code here -->");
  const [cssCode, setCssCode] = useState("/* CSS code here */");
  const [jsCode, setJsCode] = useState("// JavaScript code here");
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
        },
        body: JSON.stringify({
          title,
          html_code: htmlCode,
          css_code: cssCode,
          js_code: jsCode,
        }),
      });

      if (response.ok) {
        const result = await response.json();
        setSaveStatus({
          success: true,
          message: `Post saved with ID: ${result.id}`,
        });
        
        // Navigate to homepage after successful save
        setTimeout(() => {
          navigate('/');
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
    <div className="create-post-container">
      <div className="editor-header">
        <div className="header-left">
          <Link to="/" className="back-button">Back to Home</Link>
          <input
            type="text"
            value={title}
            onChange={(e) => setTitle(e.target.value)}
            className="title-input"
            placeholder="Enter post title..."
          />
        </div>
        <button
          className="save-button"
          onClick={handleSave}
          disabled={isSaving}
        >
          {isSaving ? "Saving..." : "Save Post"}
        </button>
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
