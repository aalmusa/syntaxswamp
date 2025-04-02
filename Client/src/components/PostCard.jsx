import { useState, useEffect } from 'react';
import { Link } from 'react-router-dom';
import '../styles/PostCard.css';

function PostCard({ post }) {
  const [previewSrc, setPreviewSrc] = useState('');
  
  useEffect(() => {
    const fetchPostDetails = async () => {
      try {
        const response = await fetch(`http://localhost:18080/posts/${post.id}`);
        if (response.ok) {
          const data = await response.json();
          const previewDoc = `
            <!DOCTYPE html>
            <html>
              <head>
                <meta name="viewport" content="width=device-width, initial-scale=1.0">
                <style>
                  /* Reset styles for consistent preview */
                  html, body {
                    margin: 0;
                    padding: 0;
                    height: 100%;
                    width: 100%;
                    overflow: hidden;
                  }
                  /* Container for proper scaling */
                  .preview-container {
                    width: 100%;
                    height: 100%;
                    display: flex;
                    align-items: center;
                    justify-content: center;
                    transform-origin: center;
                    transform: scale(0.8);
                  }
                  /* User CSS */
                  ${data.css_code}
                </style>
              </head>
              <body>
                <div class="preview-container">
                  ${data.html_code}
                </div>
                <script>${data.js_code}</script>
              </body>
            </html>
          `;
          setPreviewSrc(previewDoc);
        }
      } catch (error) {
        console.error("Error fetching post details:", error);
      }
    };
    
    fetchPostDetails();
  }, [post.id]);

  // Format date to a more readable form
  const formatDate = (dateString) => {
    const date = new Date(dateString);
    return date.toLocaleDateString('en-US', { 
      year: 'numeric', 
      month: 'short', 
      day: 'numeric' 
    });
  };

  return (
    <div className="post-card">
      <Link to={`/posts/${post.id}`} className="post-card-link">
        <div className="post-preview">
          {previewSrc ? (
            <iframe 
              srcDoc={previewSrc}
              title={`Preview of ${post.title}`}
              sandbox="allow-scripts"
              scrolling="no"
              className="preview-iframe"
            />
          ) : (
            <div className="loading-preview">Loading preview...</div>
          )}
        </div>
        <div className="post-info">
          <h3 className="post-title">{post.title}</h3>
          <p className="post-date">Created: {formatDate(post.created_at)}</p>
        </div>
      </Link>
    </div>
  );
}

export default PostCard;
