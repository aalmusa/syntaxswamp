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
        const response = await fetch("http://localhost:18080/posts");
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
  }, []);

  return (
    <div className="page-container">
      <div className="page-header">
        <h1 className="page-title">Recent Pens</h1>
      </div>

      <div className="posts-container">
        {loading ? (
          <div className="loading">Loading posts...</div>
        ) : error ? (
          <div className="error-message">{error}</div>
        ) : posts.length === 0 ? (
          <div className="no-posts">
            <p>No posts found. Create your first code pen!</p>
          </div>
        ) : (
          <div className="posts-grid">
            {posts.map((post) => (
              <PostCard key={post.id} post={post} />
            ))}
          </div>
        )}
      </div>
    </div>
  );
}

export default HomePage;
