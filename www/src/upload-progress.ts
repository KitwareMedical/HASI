import { LitElement, html, css } from 'lit';
import { customElement } from 'lit/decorators.js';
import '@material/web/button/filled-button.js';
import '@material/web/icon/icon.js';
import '@shoelace-style/shoelace/dist/components/progress-bar/progress-bar.js';

import progress from './assets/upload-progress.png';

@customElement('upload-progress')
export class UploadProgress extends LitElement {
  private sendNextStage = () => {
    this.dispatchEvent(
      new Event('next-stage', { bubbles: true, composed: true })
    );
  };

  render() {
    return html`
      <div class="title">
        <md-icon>upload_file</md-icon>
        <span>someones-scan.dcm</span>
      </div>
      <sl-progress-bar value="50"></sl-progress-bar>
      <p class="stage">Registering to atlas...</p>
      <img src=${progress} />
      <div class="upload-again">
        <md-filled-button
          label="Upload Another"
          icon="refresh"
          @click=${this.sendNextStage}
        ></md-filled-button>
      </div>
    `;
  }

  static styles = css`
    :host {
      width: 100%;
      height: 100%;
      display: flex;
      flex-direction: column;
      align-items: center;
    }

    .title {
      margin: 1rem;
      display: flex;
      align-items: center;
      font-weight: 400;
      font-size: 1.4rem;
    }

    md-icon {
      font-size: 4rem;
    }

    sl-progress-bar {
      width: 300px;
    }

    .stage {
      font-size: 1.4rem;
    }

    img {
      width: 100%;
      object-fit: contain;
      flex: 1;
    }

    .upload-again {
      margin: 2rem;
      display: flex;
      align-items: center;
    }
  `;
}

declare global {
  interface HTMLElementTagNameMap {
    'upload-progress': UploadProgress;
  }
}
