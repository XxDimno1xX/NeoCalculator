export default function Footer() {
  return (
    <footer className="footer">
      <div className="container footer-inner">
        <div className="footer-brand">
          <span className="logo-neo">Neo</span><span className="logo-calc">Calculator</span>
          <span className="footer-sub">NumOS · ESP32-S3 · Open Source</span>
        </div>
        <div className="footer-links">
          <a href="https://github.com/El-EnderJ/NeoCalculator" target="_blank" rel="noopener">GitHub</a>
          <a href="https://github.com/El-EnderJ/NeoCalculator/blob/main/docs/ROADMAP.md" target="_blank" rel="noopener">Roadmap</a>
          <a href="https://github.com/El-EnderJ/NeoCalculator/blob/main/docs/HARDWARE.md" target="_blank" rel="noopener">Hardware</a>
          <a href="https://github.com/El-EnderJ/NeoCalculator/blob/main/docs/PROJECT_BIBLE.md" target="_blank" rel="noopener">Docs</a>
        </div>
        <div className="footer-copy">© 2026 NeoCalculator · GPL v3 + CERN-OHL-S · neocalculator.tech</div>
      </div>
    </footer>
  );
}
