import { useState } from "react";
import "../styles/EditorPane.css";

function EditorPane({ language, code, onChange, label }) {
  const [isMaximized, setIsMaximized] = useState(false);

  const handleChange = (e) => {
    onChange(e.target.value);
  };

  const toggleMaximize = () => {
    setIsMaximized(!isMaximized);
  };

  return (
    <div className={`editor-pane ${isMaximized ? "maximized" : ""}`}>
      <div className="editor-header">
        <div className="editor-label">{label}</div>
        <button className="maximize-button" onClick={toggleMaximize}>
          {isMaximized ? "Minimize" : "Maximize"}
        </button>
      </div>
      <textarea
        className="code-editor"
        value={code}
        onChange={handleChange}
        spellCheck="false"
        data-language={language}
      />
    </div>
  );
}

export default EditorPane;
