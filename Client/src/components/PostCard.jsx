import { useState, useEffect } from 'react';
import { Link } from 'react-router-dom';
import { useAuth } from '../context/AuthContext';
import '../styles/PostCard.css';

function PostCard({ post }) {
  const { user } = useAuth();
  const [previewSrc, setPreviewSrc] = useState('');
  const [creator, setCreator] = useState(null);

  useEffect(() => {
    const fetchPostDetails = async () => {
      try {
        const headers = user?.token ? {
          "Authorization": `Bearer ${user.token}`
        } : {};

        // Fetch both post and creator details with auth headers
        const [postResponse, creatorResponse] = await Promise.all([
          fetch(`http://localhost:18080/posts/${post.id}`, { headers }),
          fetch(`http://localhost:18080/posts/${post.id}/creator`, { headers })
        ]);

        if (postResponse.ok) {
          const data = await postResponse.json();
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

        if (creatorResponse.ok) {
          const creatorData = await creatorResponse.json();
          setCreator(creatorData);
        }
      } catch (error) {
        console.error("Error fetching post details:", error);
      }
    };
    
    fetchPostDetails();
  }, [post.id, user]);

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
          <h3 className="post-title">
            {post.title}
            {post.isPrivate && <span className="privacy-badge">Private</span>}
          </h3>
          {creator && (
            <p className="post-creator">
              Created by <span className="creator-name">{creator.username}</span>
            </p>
          )}
          <div className="post-dates">
            <p className="post-date">Created: {formatDate(post.created_at)}</p>
            <p className="post-date">Updated: {formatDate(post.updated_at || post.created_at)}</p>
          </div>
        </div>
      </Link>
    </div>
  );
}

export default PostCard;
