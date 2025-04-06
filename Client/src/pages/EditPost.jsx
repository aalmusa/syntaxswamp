import { useState, useEffect, useCallback, useRef } from "react";
import { Link, useNavigate, useParams } from "react-router-dom";
import EditorPane from "../components/EditorPane";
import Preview from "../components/Preview";
import "../styles/CreatePost.css";
import "../styles/common.css";
import { useAuth } from "../context/AuthContext";

function EditPost() {
  const navigate = useNavigate();
  const { postId } = useParams();
  const { user } = useAuth();
  const [title, setTitle] = useState("");
  const [htmlCode, setHtmlCode] = useState("");
  const [cssCode, setCssCode] = useState("");
  const [jsCode, setJsCode] = useState("");
  const [isPrivate, setIsPrivate] = useState(false);
  const [isSaving, setIsSaving] = useState(false);
  const [saveStatus, setSaveStatus] = useState(null);
  const [loading, setLoading] = useState(true);
  const [isOwner, setIsOwner] = useState(false);
  const hasActiveLock = useRef(false);
  const lockCheckRef = useRef(null);
  const [inactiveTime, setInactiveTime] = useState(0);
  const lastActivityRef = useRef(Date.now());

  const handleActivity = useCallback(() => {
    lastActivityRef.current = Date.now();
    setInactiveTime(0);
  }, []);

  useEffect(() => {
    const verifyLockAndLoadPost = async () => {
      try {
        const lockResponse = await fetch(`http://localhost:18080/posts/${postId}/lock`, {
          headers: {
            'Authorization': `Bearer ${user.token}`
          }
        });

        const lockData = await lockResponse.json();

        if (!lockResponse.ok || !lockData.locked || !lockData.is_lock_holder) {
          navigate(`/posts/${postId}`);
          return;
        }

        hasActiveLock.current = true;

        const response = await fetch(`http://localhost:18080/posts/${postId}`, {
          headers: {
            'Authorization': `Bearer ${user.token}`
          }
        });

        if (!response.ok) {
          throw new Error('Failed to fetch post');
        }

        const data = await response.json();
        setTitle(data.title);
        setHtmlCode(data.html_code);
        setCssCode(data.css_code);
        setJsCode(data.js_code);
        setIsPrivate(data.isPrivate || false);
        setIsOwner(data.user_id === parseInt(user.userId));
        setLoading(false);
      } catch (error) {
        console.error('Error:', error);
        navigate(`/posts/${postId}`);
      }
    };

    verifyLockAndLoadPost();

    lockCheckRef.current = setInterval(async () => {
      if (!hasActiveLock.current) return;

      try {
        const response = await fetch(`http://localhost:18080/posts/${postId}/lock`, {
          headers: { 'Authorization': `Bearer ${user.token}` }
        });

        const data = await response.json();
        if (!response.ok || !data.locked || !data.is_lock_holder) {
          if (lockCheckRef.current) {
            clearInterval(lockCheckRef.current);
          }
          hasActiveLock.current = false;
          navigate(`/posts/${postId}`);
        }
      } catch (error) {
        console.error('Error verifying lock:', error);
      }
    }, 30000);

    return () => {
      if (lockCheckRef.current) {
        clearInterval(lockCheckRef.current);
      }
      if (hasActiveLock.current) {
        releaseLock();
      }
    };
  }, [postId, user.token, navigate]);

  const releaseLock = async () => {
    if (!hasActiveLock.current) return;

    try {
      const response = await fetch(`http://localhost:18080/posts/${postId}/lock`, {
        method: 'DELETE',
        headers: {
          'Authorization': `Bearer ${user.token}`
        }
      });

      if (response.ok) {
        hasActiveLock.current = false;
      }
    } catch (err) {
      console.error('Error releasing lock:', err);
    }
  };

  useEffect(() => {
    const handleBeforeUnload = (e) => {
      releaseLock();
    };

    window.addEventListener('beforeunload', handleBeforeUnload);
    return () => {
      window.removeEventListener('beforeunload', handleBeforeUnload);
    };
  }, [postId, user]);

  useEffect(() => {
    const events = ['mousemove', 'keydown', 'click', 'scroll'];
    events.forEach(event => {
      window.addEventListener(event, handleActivity);
    });

    const inactivityCheck = setInterval(() => {
      const timeSinceLastActivity = Math.floor((Date.now() - lastActivityRef.current) / 1000);
      setInactiveTime(timeSinceLastActivity);
      
      if (timeSinceLastActivity >= 300) { // 5 minutes
        clearInterval(inactivityCheck);
        releaseLock();
        navigate(`/posts/${postId}`);
      }
    }, 1000);

    return () => {
      events.forEach(event => {
        window.removeEventListener(event, handleActivity);
      });
      clearInterval(inactivityCheck);
    };
  }, [handleActivity, navigate, postId]);

  const handleSave = async () => {
    if (isSaving) return;
    setIsSaving(true);
    setSaveStatus(null);

    try {
      const response = await fetch(`http://localhost:18080/posts/${postId}`, {
        method: "PUT",
        headers: {
          "Content-Type": "application/json",
          Authorization: `Bearer ${user.token}`,
        },
        body: JSON.stringify({
          title,
          html_code: htmlCode,
          css_code: cssCode,
          js_code: jsCode,
          isPrivate,
        }),
      });

      if (response.ok) {
        await releaseLock();
        setSaveStatus({
          success: true,
          message: "Post updated successfully",
        });
        setTimeout(() => {
          navigate(`/posts/${postId}`);
        }, 1500);
      } else {
        const error = await response.text();
        setSaveStatus({ success: false, message: `Error: ${error}` });
      }
    } catch (error) {
      setSaveStatus({
        success: false,
        message: `Network error: ${error.message}`,
      });
    } finally {
      setIsSaving(false);
    }
  };

  if (loading) {
    return <div className="loading-container">Loading post...</div>;
  }

  return (
    <div className="page-container">
      <div className="page-header">
        <div className="header-left">
          <Link to={`/posts/${postId}`} className="button secondary-button">
            Back to Post
          </Link>
          <input
            type="text"
            value={title}
            onChange={(e) => setTitle(e.target.value)}
            className="title-input"
            placeholder="Enter post title..."
          />
        </div>
        <div className="header-actions">
          <div className="inactivity-timer">
            Inactive for: {Math.floor(inactiveTime / 60)}:{(inactiveTime % 60).toString().padStart(2, '0')}
          </div>
          {isOwner && (
            <div className="privacy-toggle">
              <input
                type="checkbox"
                id="privacy-toggle"
                checked={isPrivate}
                onChange={(e) => setIsPrivate(e.target.checked)}
                className="privacy-checkbox"
              />
              <label htmlFor="privacy-toggle" className="privacy-label">
                {isPrivate ? "Private Post" : "Public Post"}
              </label>
            </div>
          )}
          <button
            className="button primary-button"
            onClick={handleSave}
            disabled={isSaving}
          >
            {isSaving ? "Saving..." : "Update Post"}
          </button>
        </div>
      </div>

      {saveStatus && (
        <div
          className={`save-status ${saveStatus.success ? "success" : "error"}`}
        >
          {saveStatus.message}
        </div>
      )}

      <div className="editors-container">
        <EditorPane
          language="html"
          code={htmlCode}
          onChange={setHtmlCode}
          label="HTML"
        />
        <EditorPane
          language="css"
          code={cssCode}
          onChange={setCssCode}
          label="CSS"
        />
        <EditorPane
          language="javascript"
          code={jsCode}
          onChange={setJsCode}
          label="JS"
        />
      </div>

      <div className="preview-container">
        <h3 className="preview-header">Live Preview</h3>
        <Preview htmlCode={htmlCode} cssCode={cssCode} jsCode={jsCode} />
      </div>
    </div>
  );
}

export default EditPost;
