'use client';

import { useEffect, useState } from 'react';

export default function SponsorModal() {
  const [isOpen, setIsOpen] = useState(false);

  useEffect(() => {
    const handleOpen = (event: MouseEvent) => {
      const target = event.target as HTMLElement | null;
      if (target?.closest('[data-sponsor-open]')) {
        setIsOpen(true);
      }
    };

    document.addEventListener('click', handleOpen);
    return () => document.removeEventListener('click', handleOpen);
  }, []);

  useEffect(() => {
    if (!isOpen) {
      document.body.style.overflow = '';
      return;
    }

    const previousOverflow = document.body.style.overflow;
    document.body.style.overflow = 'hidden';

    return () => {
      document.body.style.overflow = previousOverflow;
    };
  }, [isOpen]);

  useEffect(() => {
    if (!isOpen) return;

    const handleKeydown = (event: KeyboardEvent) => {
      if (event.key === 'Escape') {
        setIsOpen(false);
      }
    };

    window.addEventListener('keydown', handleKeydown);
    return () => window.removeEventListener('keydown', handleKeydown);
  }, [isOpen]);

  const closeModal = () => setIsOpen(false);

  return (
    <div
      className={`modal-overlay${isOpen ? ' open' : ''}`}
      id="sponsorModal"
      onClick={closeModal}
      aria-hidden={!isOpen}
    >
      <div className="modal-box" onClick={(event) => event.stopPropagation()}>
        <button type="button" className="modal-close" onClick={closeModal}>
          ×
        </button>
        <h3 className="modal-title">Support the Revolution</h3>
        <p className="modal-sub">Choose a platform to back the open-source hardware movement.</p>

        <div className="modal-options">
          <div className="modal-option">
            <h4>GitHub Sponsors</h4>
            <p>Support directly through GitHub to help fund development.</p>
            <iframe
              src="https://github.com/sponsors/El-EnderJ/card"
              title="Sponsor El-EnderJ"
              height="225"
              width="100%"
              style={{ border: 0, borderRadius: '6px', overflow: 'hidden', maxWidth: '600px' }}
            />
          </div>

          <div className="modal-divider" />

          <div className="modal-option">
            <h4>Ko-fi</h4>
            <p>Make a one-time donation or become a member.</p>
            <iframe
              id="kofiframe"
              src="https://ko-fi.com/enderdesigns/?hidefeed=true&widget=true&embed=true&preview=true"
              style={{ border: 'none', width: '100%', padding: 0, background: 'transparent', borderRadius: '8px' }}
              height="712"
              title="enderdesigns"
            />
          </div>
        </div>
      </div>
    </div>
  );
}
