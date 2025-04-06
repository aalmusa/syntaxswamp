import { useState, useRef, useEffect } from "react";
import CodeMirror from "@uiw/react-codemirror";
import { html } from "@codemirror/lang-html";
import { css } from "@codemirror/lang-css";
import { javascript } from "@codemirror/lang-javascript";
import { oneDark } from "@codemirror/theme-one-dark";
import "../styles/EditorPane.css";

function EditorPane({ language, code, onChange, label, readOnly = false }) {
  const [isMaximized, setIsMaximized] = useState(false);
  const [copySuccess, setCopySuccess] = useState(false);
  const [isResizing, setIsResizing] = useState(false);
  const [height, setHeight] = useState(300); // Default height
  const editorRef = useRef(null);
  const startYRef = useRef(0);
  const startHeightRef = useRef(0);

  // Choose the appropriate language extension based on the language prop
  const getLanguageExtension = () => {
    switch (language) {
      case "html":
        return html();
      case "css":
        return css();
      case "javascript":
        return javascript();
      default:
        return html();
    }
  };

  const toggleMaximize = () => {
    setIsMaximized(!isMaximized);
  };

  const handleCopyClick = async () => {
    try {
      await navigator.clipboard.writeText(code);
      setCopySuccess(true);

      // Reset the success state after 2 seconds
      setTimeout(() => {
        setCopySuccess(false);
      }, 2000);
    } catch (err) {
      console.error("Failed to copy code: ", err);
    }
  };

  const startResize = (e) => {
    setIsResizing(true);
    startYRef.current = e.clientY;
    startHeightRef.current = height;
    document.body.style.cursor = "row-resize";
    document.body.style.userSelect = "none"; // Prevent text selection while resizing
  };

  const handleResize = (e) => {
    if (!isResizing) return;
    const deltaY = e.clientY - startYRef.current;
    const newHeight = Math.max(100, startHeightRef.current + deltaY);
    setHeight(newHeight);
  };

  const stopResize = () => {
    if (isResizing) {
      setIsResizing(false);
      document.body.style.cursor = "";
      document.body.style.userSelect = "";
    }
  };

  useEffect(() => {
    if (isResizing) {
      document.addEventListener("mousemove", handleResize);
      document.addEventListener("mouseup", stopResize);
      return () => {
        document.removeEventListener("mousemove", handleResize);
        document.removeEventListener("mouseup", stopResize);
      };
    }
  }, [isResizing]);

  return (
    <div
      className={`editor-pane ${isMaximized ? "maximized" : ""} ${
        readOnly ? "read-only" : ""
      }`}
      ref={editorRef}
      style={{ height: `${height}px` }}
    >
      <div className="editor-header">
        <div className="editor-label">{label}</div>
        <div className="editor-actions">
          <button
            className={`copy-button ${copySuccess ? "success" : ""}`}
            onClick={handleCopyClick}
          >
            {copySuccess ? "Copied!" : "Copy"}
          </button>
          <button className="maximize-button" onClick={toggleMaximize}>
            {isMaximized ? "Minimize" : "Maximize"}
          </button>
        </div>
      </div>

      <div className="code-editor" data-language={language}>
        <CodeMirror
          value={code}
          height="100%"
          theme={oneDark}
          extensions={[getLanguageExtension()]}
          onChange={(value) => !readOnly && onChange(value)}
          readOnly={readOnly}
          basicSetup={{
            lineNumbers: true,
            highlightActiveLineGutter: true,
            highlightSpecialChars: true,
            foldGutter: true,
            dropCursor: true,
            allowMultipleSelections: !readOnly,
            indentOnInput: !readOnly,
            syntaxHighlighting: true,
            bracketMatching: true,
            closeBrackets: !readOnly,
            autocompletion: !readOnly,
            rectangularSelection: !readOnly,
            crosshairCursor: !readOnly,
            highlightActiveLine: !readOnly,
            highlightSelectionMatches: true,
            closeBracketsKeymap: !readOnly,
            searchKeymap: !readOnly,
          }}
        />
      </div>
      <div className="resize-handle" onMouseDown={startResize}>
        <div className="resize-handle-bar"></div>
      </div>
    </div>
  );
}

export default EditorPane;
