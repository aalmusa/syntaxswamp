import { useState, useRef, useEffect } from "react";
import Preview from "./Preview";
import "../styles/ResizablePreview.css";

function ResizablePreview({ htmlCode, cssCode, jsCode }) {
  // Set initial height to 40% of viewport height for consistency
  const [height, setHeight] = useState(Math.max(300, window.innerHeight * 0.4));
  const [isResizing, setIsResizing] = useState(false);
  const [isExpanded, setIsExpanded] = useState(false);
  const startYRef = useRef(0);
  const startHeightRef = useRef(0);
  const previewRef = useRef(null);
  const containerRef = useRef(null);
  const maxHeightRef = useRef(0);

  // Calculate maximum possible height on mount and window resize
  useEffect(() => {
    const calculateMaxHeight = () => {
      const editorContainerHeight =
        document.querySelector(".editors-container")?.offsetHeight || 0;
      const viewportHeight = window.innerHeight;

      // Set max height to be 80% of the viewport height
      maxHeightRef.current = Math.floor(viewportHeight * 0.8);

      // Also update current height if the window is resized
      if (!isResizing && !isExpanded) {
        setHeight(Math.max(300, Math.floor(viewportHeight * 0.4)));
      }
    };

    calculateMaxHeight();
    window.addEventListener("resize", calculateMaxHeight);
    return () => window.removeEventListener("resize", calculateMaxHeight);
  }, [isResizing, isExpanded]);

  const startResize = (e) => {
    e.preventDefault();
    setIsResizing(true);
    startYRef.current = e.clientY;
    startHeightRef.current = height;
    document.body.style.cursor = "ns-resize";
    document.body.style.userSelect = "none"; // Prevent text selection while resizing
  };

  const handleResize = (e) => {
    if (!isResizing) return;
    const deltaY = startYRef.current - e.clientY;

    // Calculate new height with more generous max height
    const newHeight = Math.max(200, startHeightRef.current + deltaY);

    // If the height gets close to max, auto-expand to full size
    if (newHeight > maxHeightRef.current * 0.8) {
      setIsExpanded(true);
      setHeight(maxHeightRef.current);
    } else {
      setIsExpanded(false);
      setHeight(newHeight);
    }
  };

  const stopResize = () => {
    if (isResizing) {
      setIsResizing(false);
      document.body.style.cursor = "";
      document.body.style.userSelect = "";
    }
  };

  const toggleExpand = () => {
    if (isExpanded) {
      setIsExpanded(false);
      setHeight(300);
    } else {
      setIsExpanded(true);
      setHeight(maxHeightRef.current);
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
      className={`resizable-preview-wrapper ${isExpanded ? "expanded" : ""}`}
      style={{ height: isExpanded ? "100%" : `${height}px` }}
      ref={containerRef}
    >
      <div className="preview-resize-handle" onMouseDown={startResize}>
        <div className="handle-bar"></div>
      </div>
      <div className="preview-content">
        <div className="preview-header-container">
          <h3 className="preview-header">Preview</h3>
          <button className="preview-expand-button" onClick={toggleExpand}>
            {isExpanded ? "Minimize" : "Maximize"}
          </button>
        </div>
        <div className="preview-frame-container" ref={previewRef}>
          <Preview htmlCode={htmlCode} cssCode={cssCode} jsCode={jsCode} />
        </div>
      </div>
    </div>
  );
}

export default ResizablePreview;
