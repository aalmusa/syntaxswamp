import { BrowserRouter as Router, Routes, Route } from 'react-router-dom';
import "./App.css";
import Navbar from './components/Navbar';
import HomePage from './pages/HomePage';
import CreatePost from "./pages/CreatePost";
import ViewPost from "./pages/ViewPost";
import EditPost from './pages/EditPost';

function App() {
  return (
    <Router>
      <div className="app-container">
        <Navbar />
        <main className="main-content">
          <Routes>
            <Route path="/" element={<HomePage />} />
            <Route path="/posts/new" element={<CreatePost />} />
            <Route path="/posts/:postId" element={<ViewPost />} />
            <Route path="/posts/:postId/edit" element={<EditPost />} />
          </Routes>
        </main>
      </div>
    </Router>
  );
}

export default App;
