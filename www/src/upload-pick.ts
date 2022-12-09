import { LitElement, html, css } from 'lit';
import { customElement } from 'lit/decorators.js';
import '@material/web/button/filled-button.js';

@customElement('upload-pick')
export class UploadPick extends LitElement {
  private sendNextStage = () => {
    this.dispatchEvent(
      new Event('next-stage', { bubbles: true, composed: true })
    );
  };

  render() {
    return html`
      <div>
        <p>Drag scan file here or</p>
        <md-filled-button
          label="Open File"
          icon="file_open"
          @click=${this.sendNextStage}
        ></md-filled-button>
      </div>
    `;
  }

  static styles = css`
    :host {
      width: 100%;
      display: flex;
      flex-direction: column;
      align-items: center;
      justify-content: center;

      background: repeating-linear-gradient(
        45deg,
        #ffffff,
        #ffffff 60px,
        #ebebeb 60px,
        #ebebeb 120px
      );
    }

    :host > div {
      display: flex;
      flex-direction: column;
      justify-content: center;
    }

    p {
      text-align: center;
      font-weight: 400;
      font-size: 1.4rem;
    }
  `;
}

declare global {
  interface HTMLElementTagNameMap {
    'upload-pick': UploadPick;
  }
}
