import { Link } from 'react-router-dom';
import { useAuth } from '../context/AuthContext';
import '../styles/Navbar.css';

function Navbar() {
  const { user, logout } = useAuth();

  return (
    <nav className="navbar">
      <Link to="/" className="nav-brand">SyntaxSwamp</Link>
      {user && (
        <div className="nav-actions">
          <Link to="/posts/new" className="create-button">Create New</Link>
          <div className="user-info">
            <span className="username">{user.username}</span>
            <button onClick={logout} className="logout-button">Logout</button>
          </div>
        </div>
      )}
    </nav>
  );
}

export default Navbar;
