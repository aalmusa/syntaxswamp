import { useLocation, Link } from 'react-router-dom';
import { useAuth } from '../context/AuthContext';
import '../styles/Header.css';

function Header() {
  const { user, logout } = useAuth();
  const location = useLocation();
  const isHomePage = location.pathname === '/';

  return (
    <header className="header">
      <div className="header-left">
        <Link to="/" className="site-title">CodePen Clone</Link>
      </div>
      <div className="header-right">
        {isHomePage ? (
          // Only show these buttons on homepage
          user ? (
            <>
              <Link to="/create" className="button primary-button">Create New</Link>
              <button onClick={logout} className="button secondary-button">Logout</button>
            </>
          ) : (
            <>
              <Link to="/login" className="button primary-button">Login</Link>
              <Link to="/register" className="button secondary-button">Register</Link>
            </>
          )
        ) : (
          // On other pages, just show home button
          <Link to="/" className="button secondary-button">Home</Link>
        )}
      </div>
    </header>
  );
}

export default Header;