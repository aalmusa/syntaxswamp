import { Link } from 'react-router-dom';
import '../styles/Navbar.css';

function Navbar() {
  return (
    <nav className="navbar">
      <div className="container">
        <Link to="/" className="nav-brand">
          <span className="brand-text">SyntaxSwamp</span>
        </Link>
        <div className="nav-items">
          <Link to="/" className="nav-item">Home</Link>
          <Link to="/create" className="nav-item button">Create Post</Link>
        </div>
      </div>
    </nav>
  );
}

export default Navbar;
