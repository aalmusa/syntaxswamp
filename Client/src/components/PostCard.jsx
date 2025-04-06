import { useState, useEffect } from 'react';
import { Link, useNavigate } from 'react-router-dom';
import { useAuth } from '../context/AuthContext';
import '../styles/PostCard.css';

function PostCard({ post, lockInfo, onEdit, onEditComplete }) {
  const { user } = useAuth();
  const navigate = useNavigate();
  const [previewSrc, setPreviewSrc] = useState('');
  const [creator, setCreator] = useState(null);
  const [remainingTime, setRemainingTime] = useState(null);

  useEffect(() => {
    const fetchPostDetails = async () => {
      try {
        const headers = user?.token ? {
          "Authorization": `Bearer ${user.token}`
        } : {};

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
                <base target="_blank">
                <meta charset="utf-8">
                <meta name="viewport" content="width=device-width, initial-scale=1.0">
                <style>
                  html, body {
                    margin: 0;
                    padding: 0;
                    height: 100%;
                    width: 100%;
                    overflow: hidden;
                  }
                  .preview-container {
                    width: 100%;
                    height: 100%;
                    display: flex;
                    align-items: center;
                    justify-content: center;
                    transform-origin: center;
                    transform: scale(0.8);
                  }
                  /* Inject user CSS directly */
                  ${data.css_code}
                </style>
              </head>
              <body>
                <div class="preview-container">
                  ${data.html_code}
                </div>
                <script type="text/javascript">
                  // Wrap JS in IIFE to avoid global scope pollution
                  (function() {
                    ${data.js_code}
                  })();
                </script>
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

  useEffect(() => {
    let timer;
    if (lockInfo?.locked && lockInfo.seconds_remaining > 0) {
      setRemainingTime(lockInfo.seconds_remaining);
      
      timer = setInterval(() => {
        setRemainingTime(prev => {
          if (prev <= 1) {
            clearInterval(timer);
            return 0;
          }
          return prev - 1;
        });
      }, 1000);
    }

    return () => {
      if (timer) clearInterval(timer);
    };
  }, [lockInfo]);

  useEffect(() => {
    return () => {
      if (lockInfo?.isHolder) {
        onEditComplete();
      }
    };
  }, [lockInfo, onEditComplete]);

  const formatDate = (dateString) => {
    const date = new Date(dateString);
    return date.toLocaleDateString('en-US', { 
      year: 'numeric', 
      month: 'short', 
      day: 'numeric' 
    });
  };

  const formatTime = (seconds) => {
    if (!seconds) return '';
    const minutes = Math.floor(seconds / 60);
    const remainingSeconds = seconds % 60;
    return `${minutes}:${remainingSeconds.toString().padStart(2, '0')}`;
  };

  const handlePostClick = (e) => {
    // Let Link handle navigation normally
  };

  return (
    <div className={`post-card ${lockInfo?.locked ? 'locked' : ''}`}>
      <Link to={`/posts/${post.id}`} className="post-card-link" onClick={handlePostClick}>
        <div className="post-preview">
          {previewSrc ? (
            <iframe 
              srcDoc={previewSrc}
              title={`Preview of ${post.title}`}
              sandbox="allow-scripts"
              scrolling="no"
              className="preview-iframe"
              style={{ border: 'none', width: '100%', height: '100%' }}
              referrerPolicy="no-referrer"
            />
          ) : (
            <div className="loading-preview">Loading preview...</div>
          )}
          {lockInfo?.locked && (
            <div className="lock-overlay">
              <div className="lock-info">
                {lockInfo.isHolder ? (
                  <>
                    <span className="lock-status">You are editing</span>
                    <span className="lock-timer">{formatTime(remainingTime)}</span>
                  </>
                ) : (
                  <>
                    <span className="lock-status">Being edited by {lockInfo.username}</span>
                    <span className="lock-timer">{formatTime(remainingTime)}</span>
                  </>
                )}
              </div>
            </div>
          )}
        </div>
        <div className="post-info">
          <h3 className="post-title">
            {post.title}
            {post.isPrivate && <span className="privacy-badge">Private</span>}
            {lockInfo?.locked && (
              <span className={`lock-badge ${lockInfo.isHolder ? 'your-lock' : ''}`}>
                {lockInfo.isHolder ? 'Editing' : 'Locked'}
              </span>
            )}
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
