import { useMemo } from "react";
import "../styles/Preview.css";

function Preview({ htmlCode, cssCode, jsCode }) {
  const srcDoc = useMemo(() => {
    return `
      <!DOCTYPE html>
      <html>
        <head>
          <meta name="viewport" content="width=device-width, initial-scale=1.0">
          <style>
            /* Base styles to ensure content is properly displayed */
            html, body {
              margin: 0;
              padding: 0;
              height: 100%;
              width: 100%;
              overflow: auto;
            }
            
            /* Wrapper to ensure proper display */
            .content-wrapper {
              min-height: 100%;
              padding: 15px;
            }
            
            /* User CSS */
            ${cssCode}
          </style>
        </head>
        <body>
          <div class="content-wrapper">
            ${htmlCode}
          </div>
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
