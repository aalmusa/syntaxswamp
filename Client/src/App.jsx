import { BrowserRouter as Router, Routes, Route } from 'react-router-dom';
import Navbar from './components/Navbar';
import HomePage from './pages/HomePage';
import CreatePost from './pages/CreatePost';
import ViewPost from './pages/ViewPost';
import EditPost from './pages/EditPost';

function App() {
  return (
    <Router>
      <Navbar />
      <main>
        <Routes>
          <Route path="/" element={<HomePage />} />
          <Route path="/create" element={<CreatePost />} />
          <Route path="/posts/:postId" element={<ViewPost />} />
          <Route path="/posts/:postId/edit" element={<EditPost />} />
        </Routes>
      </main>
    </Router>
  );
}

export default App;
