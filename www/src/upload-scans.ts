import { LitElement, html, css } from 'lit';
import { customElement } from 'lit/decorators.js';
import '@material/web/button/filled-button.js';

@customElement('upload-scans')
export class UploadScans extends LitElement {
  render() {
    return html`
      <div>
        <p>Drag scan files here or</p>
        <md-filled-button
          label="Open Files"
          icon="file_open"
        ></md-filled-button>
      </div>
      <div>
        <md-filled-button label="Upload" icon="upload"></md-filled-button>
      </div>
    `;
  }

  static styles = css`
    :host {
      display: flex;
      flex-direction: column;
      align-items: center;
    }

    :host > div {
      flex: 1;

      display: flex;
      flex-direction: column;
      justify-content: center;
    }

    p {
      text-align: center;
    }

    md-filled-button {
      width: 14rem;
    }
  `;
}

declare global {
  interface HTMLElementTagNameMap {
    'upload-scans': UploadScans;
  }
}
