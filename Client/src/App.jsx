import { BrowserRouter as Router, Routes, Route } from 'react-router-dom';
import "./App.css";
import Navbar from './components/Navbar';
import HomePage from './pages/HomePage';
import CreatePost from "./pages/CreatePost";
import ViewPost from "./pages/ViewPost";

function App() {
  return (
    <Router>
      <div className="app-container">
        <Navbar />
        <main className="main-content">
          <Routes>
            <Route path="/" element={<HomePage />} />
            <Route path="/create" element={<CreatePost />} />
            <Route path="/view/:postId" element={<ViewPost />} />
          </Routes>
        </main>
      </div>
    </Router>
  );
}

export default App;
