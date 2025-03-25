import { Link } from 'react-router-dom';
import '../styles/Navbar.css';

function Navbar() {
  return (
    <nav className="navbar">
      <div className="nav-container">
        <Link to="/" className="nav-logo">Syntax Swamp</Link>
        <div className="nav-links">
          <Link to="/" className="nav-link">Home</Link>
          <Link to="/create" className="nav-button">Create New Pen</Link>
        </div>
      </div>
    </nav>
  );
}

export default Navbar;
