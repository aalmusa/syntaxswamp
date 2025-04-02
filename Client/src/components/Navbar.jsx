import { Link } from 'react-router-dom';
import { useAuth } from '../context/AuthContext';
import '../styles/Navbar.css';

function Navbar() {
  const { user, logout } = useAuth();

  return (
    <nav className="navbar">
      <div className="nav-section">
        <Link to="/" className="nav-brand">SyntaxSwamp</Link>
      </div>
      {user && (
        <>
          <div className="nav-section nav-center">
            <div className="user-welcome">
              Welcome, <span className="username">{user.username}</span>
            </div>
          </div>
          <div className="nav-section">
            <Link to="/posts/new" className="create-button">Create New</Link>
            <button onClick={logout} className="logout-button">Logout</button>
          </div>
        </>
      )}
    </nav>
  );
}

export default Navbar;
