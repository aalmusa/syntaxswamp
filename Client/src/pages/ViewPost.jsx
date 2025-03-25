import { useState, useEffect } from 'react';
import { useParams } from 'react-router-dom';
import EditorPane from '../components/EditorPane';
import Preview from '../components/Preview';
import '../styles/ViewPost.css';

function ViewPost() {
  const { postId } = useParams();
  const [post, setPost] = useState(null);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState(null);
  
  useEffect(() => {
    const fetchPost = async () => {
      try {
        setLoading(true);
        const response = await fetch(`http://localhost:18080/posts/${postId}`);
        
        if (!response.ok) {
          throw new Error(`Error: ${response.status}`);
        }
        
        const data = await response.json();
        setPost(data);
        setError(null);
      } catch (err) {
        setError('Failed to load post. Please try again later.');
        console.error('Error fetching post:', err);
      } finally {
        setLoading(false);
      }
    };
    
    fetchPost();
  }, [postId]);
  
  if (loading) {
    return <div className="loading-container">Loading post...</div>;
  }
  
  if (error || !post) {
    return (
      <div className="error-container">
        <h2>Error</h2>
        <p>{error || "Post not found"}</p>
      </div>
    );
  }
  
  return (
    <div className="view-post-container">
      <div className="view-header">
        <h2 className="post-title">{post.title}</h2>
      </div>
      
      <div className="post-content">
        <div className="editors-container">
          <EditorPane
            language="html"
            code={post.html_code}
            onChange={() => {}} // Read-only in view mode
            label="HTML"
          />
          <EditorPane
            language="css"
            code={post.css_code}
            onChange={() => {}} // Read-only in view mode
            label="CSS"
          />
          <EditorPane
            language="javascript"
            code={post.js_code}
            onChange={() => {}} // Read-only in view mode
            label="JS"
          />
        </div>
        
        <div className="preview-container">
          <h3 className="preview-header">Preview</h3>
          <Preview
            htmlCode={post.html_code}
            cssCode={post.css_code}
            jsCode={post.js_code}
          />
        </div>
      </div>
    </div>
  );
}

export default ViewPost;
