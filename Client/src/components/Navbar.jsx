import { Link, useLocation } from 'react-router-dom';
import { useAuth } from '../context/AuthContext';
import '../styles/Navbar.css';

function Navbar() {
  const { user, logout } = useAuth();
  const location = useLocation();
  const isHomePage = location.pathname === '/';

  return (
    <nav className="navbar">
      <div className="nav-section">
        <Link to="/" className="nav-brand">SyntaxSwamp</Link>
      </div>
      {user ? (
        <>
          <div className="nav-section nav-center">
            <div className="user-welcome">
              Welcome, <span className="username">{user.username}</span>
            </div>
          </div>
          {isHomePage ? (
            <div className="nav-section">
              <Link to="/posts/new" className="create-button">Create New</Link>
              <button onClick={logout} className="logout-button">Logout</button>
            </div>
          ) : (
            <></>
          )}
        </>
      ) : (
        <div className="nav-section">
          {isHomePage ? (
            <>
              <Link to="/login" className="nav-link">Login</Link>
              <Link to="/register" className="nav-link">Register</Link>
            </>
          ) : (
            <></>
          )}
        </div>
      )}
    </nav>
  );
}

export default Navbar;
