import "../styles/Preview.css";

function Preview({ htmlCode, cssCode, jsCode }) {
  const previewDoc = `
    <!DOCTYPE html>
    <html>
      <head>
        <meta charset="utf-8">
        <style>
          ${cssCode}
        </style>
      </head>
      <body>
        ${htmlCode}
        <script type="module">
          ${jsCode}
        </script>
      </body>
    </html>
  `;

  return (
    <iframe
      srcDoc={previewDoc}
      title="Preview"
      sandbox="allow-scripts"
      className="preview-frame"
      style={{ border: 'none', width: '100%', height: '100%' }}
    />
  );
}

export default Preview;
