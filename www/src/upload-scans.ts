import { LitElement, css } from 'lit';
import { customElement, state } from 'lit/decorators.js';
import { html, unsafeStatic } from 'lit/static-html.js';

import './upload-pick.js';
import './upload-preview.js';
import './upload-progress.js';

const UPLOAD_STAGES = [
  'upload-pick',
  'upload-preview',
  'upload-progress',
] as const;
type UploadStage = typeof UPLOAD_STAGES[number];

const STAGE_COMPONENTS = UPLOAD_STAGES.map((tag) => unsafeStatic(`${tag}`));

@customElement('upload-scans')
export class UploadScans extends LitElement {
  @state() uploadStage: UploadStage = UPLOAD_STAGES[0];

  private progressStage = (e: CustomEvent) => {
    e.stopPropagation();
    this.uploadStage =
      UPLOAD_STAGES[
        (UPLOAD_STAGES.indexOf(this.uploadStage) + 1) % UPLOAD_STAGES.length
      ];
  };

  render() {
    const uploadStageTag =
      STAGE_COMPONENTS[UPLOAD_STAGES.indexOf(this.uploadStage)];
    return html`
      <div @next-stage=${this.progressStage}>
        <${uploadStageTag} class="uploadStage"> </${uploadStageTag}>
      </div>
    `;
  }

  static styles = css`
    div {
      height: 100%;
      display: flex;
      flex-direction: column;
      align-items: center;
      --md-filled-button-label-text-size: 1.4rem;
    }

    .uploadStage {
      flex: 1;
    }
  `;
}

declare global {
  interface HTMLElementTagNameMap {
    'upload-scans': UploadScans;
  }
}
