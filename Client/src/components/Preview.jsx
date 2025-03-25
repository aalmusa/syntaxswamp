import { useMemo } from "react";
import "../styles/Preview.css";

function Preview({ htmlCode, cssCode, jsCode }) {
  const srcDoc = useMemo(() => {
    return `
      <!DOCTYPE html>
      <html>
        <head>
          <style>${cssCode}</style>
        </head>
        <body>
          ${htmlCode}
          <script>${jsCode}</script>
        </body>
      </html>
    `;
  }, [htmlCode, cssCode, jsCode]);

  return (
    <div className="preview">
      <iframe
        srcDoc={srcDoc}
        className="preview-iframe"
        title="Code Preview"
        sandbox="allow-scripts"
      />
    </div>
  );
}

export default Preview;
