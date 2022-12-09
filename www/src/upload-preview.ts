import { LitElement, html, css } from 'lit';
import { customElement } from 'lit/decorators.js';
import '@material/web/button/filled-button.js';

import './scan-view.js';

@customElement('upload-preview')
export class UploadPreview extends LitElement {
  private sendNextStage = () => {
    this.dispatchEvent(
      new Event('next-stage', { bubbles: true, composed: true })
    );
  };

  render() {
    return html`
      <scan-view></scan-view>
      <div>
        <md-filled-button
          label="Upload and Process"
          icon="settings"
          @click=${this.sendNextStage}
        ></md-filled-button>
      </div>
    `;
  }

  static styles = css`
    :host {
      display: flex;
      flex-direction: column;
      align-items: center;
      width: 100%;
    }

    scan-view {
      flex: 1;
      width: 100%;
    }
    md-filled-button {
      margin: 0.4rem;
    }
  `;
}

declare global {
  interface HTMLElementTagNameMap {
    'upload-preview': UploadPreview;
  }
}
