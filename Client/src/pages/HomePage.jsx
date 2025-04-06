import { useState, useEffect } from "react";
import { useAuth } from "../context/AuthContext";
import PostCard from "../components/PostCard";
import "../styles/common.css";
import "../styles/HomePage.css";

const formatDate = (dateString) => {
  return new Date(dateString).toLocaleDateString("en-US", {
    year: "numeric",
    month: "short",
    day: "numeric",
  });
};

function HomePage() {
  const { user } = useAuth();
  const [posts, setPosts] = useState([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState(null);
  const [lockedPosts, setLockedPosts] = useState(new Map());

  // Debug: Verify authentication status when component mounts
  useEffect(() => {
    if (user) {
      console.log(
        "Authenticated as user:",
        user.userId,
        "with token:",
        user.token?.substring(0, 10) + "..."
      );
    } else {
      console.log("Not authenticated");
    }
  }, [user]);

  useEffect(() => {
    const fetchPosts = async () => {
      try {
        setLoading(true);
        const headers = {};
        if (user?.token) {
          headers["Authorization"] = `Bearer ${user.token}`;
        }

        const response = await fetch("http://localhost:18080/posts", {
          headers,
        });

        if (!response.ok) {
          throw new Error(`Error: ${response.status}`);
        }
        const data = await response.json();
        setPosts(data.posts || []);
        setError(null);
      } catch (err) {
        setError("Failed to load posts. Please try again later.");
        console.error("Error fetching posts:", err);
      } finally {
        setLoading(false);
      }
    };

    fetchPosts();
  }, [user]);

  const acquireLock = async (postId) => {
    if (!user?.token) return false;

    try {
      const response = await fetch(
        `http://localhost:18080/posts/${postId}/lock`,
        {
          method: "POST",
          headers: {
            Authorization: `Bearer ${user.token}`,
            "Content-Type": "application/json",
          },
          body: JSON.stringify({ duration: 300 }), // 5 minutes lock
        }
      );

      const data = await response.json();

      if (response.ok) {
        setLockedPosts((prev) =>
          new Map(prev).set(postId, {
            locked: true,
            isHolder: true,
            lockHolder: data.lock_holder,
            expiresAt: data.expires_at,
          })
        );
        return true;
      } else if (response.status === 423) {
        // Locked by someone else
        setLockedPosts((prev) =>
          new Map(prev).set(postId, {
            locked: true,
            isHolder: false,
            lockHolder: data.lock_holder,
            expiresAt: data.expires_at,
          })
        );
        return false;
      }
      return false;
    } catch (err) {
      console.error("Error acquiring lock:", err);
      return false;
    }
  };

  const releaseLock = async (postId) => {
    if (!user?.token) return;

    try {
      await fetch(`http://localhost:18080/posts/${postId}/lock`, {
        method: "DELETE",
        headers: {
          Authorization: `Bearer ${user.token}`,
        },
      });
      setLockedPosts((prev) => {
        const newLocks = new Map(prev);
        newLocks.delete(postId);
        return newLocks;
      });
    } catch (err) {
      console.error("Error releasing lock:", err);
    }
  };

  // Add periodic lock status check
  useEffect(() => {
    if (!user) return; // Only check locks if user is logged in

    const checkLocksStatus = async () => {
      // Make a copy of current locks to check
      const currentLocks = new Map(lockedPosts);
      for (const [postId] of currentLocks) {
        const status = await checkLockStatus(postId);
        if (!status?.locked) {
          // Remove expired locks
          setLockedPosts((prev) => {
            const newLocks = new Map(prev);
            newLocks.delete(postId);
            return newLocks;
          });
        }
      }
    };

    const interval = setInterval(checkLocksStatus, 10000); // Check every 10 seconds
    return () => clearInterval(interval);
  }, [user, lockedPosts]);

  // Modified checkLockStatus to handle expired locks
  const checkLockStatus = async (postId) => {
    try {
      const response = await fetch(
        `http://localhost:18080/posts/${postId}/lock`,
        {
          headers: user?.token
            ? {
                Authorization: `Bearer ${user.token}`,
              }
            : {},
        }
      );

      if (response.ok) {
        const data = await response.json();
        if (!data.locked) {
          // Clear the lock if it's expired
          setLockedPosts((prev) => {
            const newLocks = new Map(prev);
            newLocks.delete(postId);
            return newLocks;
          });
        } else {
          setLockedPosts((prev) =>
            new Map(prev).set(postId, {
              locked: true,
              isHolder: data.is_lock_holder,
              lockHolder: data.username,
              seconds_remaining: data.seconds_remaining,
            })
          );
        }
        return data;
      }
    } catch (err) {
      console.error("Error checking lock status:", err);
    }
    return null;
  };

  const handleEdit = async (postId) => {
    const lock = await acquireLock(postId);
    if (!lock) {
      const status = await checkLockStatus(postId);
      if (status?.locked) {
        alert(`This post is being edited by ${status.username}`);
      } else {
        alert("Unable to edit post at this time");
      }
      return false;
    }
    return true;
  };

  // Add cleanup effect when user leaves the page
  useEffect(() => {
    return () => {
      // Release all locks held by current user when leaving page
      if (user) {
        lockedPosts.forEach((lock, postId) => {
          if (lock.isHolder) {
            releaseLock(postId);
          }
        });
      }
    };
  }, [user, lockedPosts]);

  // Add auto-release timer for each lock
  useEffect(() => {
    const timers = new Map();

    lockedPosts.forEach((lock, postId) => {
      if (lock.isHolder) {
        // Set timer to release lock after 5 minutes of inactivity
        const timer = setTimeout(() => {
          releaseLock(postId);
        }, 5 * 60 * 1000); // 5 minutes
        timers.set(postId, timer);
      }
    });

    return () => {
      // Clear all timers on cleanup
      timers.forEach((timer) => clearTimeout(timer));
    };
  }, [lockedPosts]);

  return (
    <div className="page-container">
      <div className="page-header">
        <h1 className="page-title">Recent Posts</h1>
      </div>

      <div className="posts-container">
        {loading ? (
          <div className="loading">Loading posts...</div>
        ) : error ? (
          <div className="error-message">{error}</div>
        ) : posts.length === 0 ? (
          <div className="no-posts">
            <p>No posts found. Create your first syntax swamp!</p>
          </div>
        ) : (
          <div className="posts-container">
            <div className="posts-grid">
              {posts.map((post) => (
                <PostCard
                  key={post.id}
                  post={post}
                  user={user}
                  lockInfo={lockedPosts.get(post.id)}
                  onEdit={() => handleEdit(post.id)}
                  onEditComplete={() => releaseLock(post.id)}
                />
              ))}
            </div>
          </div>
        )}
      </div>
    </div>
  );
}

export default HomePage;
