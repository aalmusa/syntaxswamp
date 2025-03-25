import { useState } from "react";
import reactLogo from "./assets/react.svg";
import viteLogo from "/vite.svg";
import "./App.css";
import CreatePost from "./pages/CreatePost";

function App() {
  const [showEditor, setShowEditor] = useState(false);

  return (
    <>
      {showEditor ? (
        <CreatePost />
      ) : (
        <div className="welcome-screen">
          <div>
            <a href="https://vite.dev" target="_blank">
              <img src={viteLogo} className="logo" alt="Vite logo" />
            </a>
            <a href="https://react.dev" target="_blank">
              <img src={reactLogo} className="logo react" alt="React logo" />
            </a>
          </div>
          <h1>Syntax Swamp</h1>
          <div className="card">
            <button onClick={() => setShowEditor(true)}>
              Open Code Editor
            </button>
            <p>Create and share HTML, CSS, and JavaScript code snippets.</p>
          </div>
        </div>
      )}
    </>
  );
}

export default App;
