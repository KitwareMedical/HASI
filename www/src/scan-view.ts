import { LitElement, css, html } from 'lit';
import { customElement, property } from 'lit/decorators.js';
import { styleMap } from 'lit/directives/style-map.js';
import { ContextConsumer } from '@lit-labs/context';
import '@material/web/button/elevated-button.js';

import { Feature } from './scan.types.js';
import { ScanSelection } from './state/scan-selections.js';
import { hasiContext } from './state/hasi.machine.js';

import mesh from './assets/mesh.png';

@customElement('scan-view')
export class ScanView extends LitElement {
  @property() scan?: ScanSelection;
  @property() feature!: Feature;

  stateService = new ContextConsumer(this, hasiContext, undefined, true);

  focusScan() {
    if (this.scan)
      this.stateService.value?.service.send({
        type: 'FOCUS_SCAN',
        id: this.scan.id,
      });
  }

  render() {
    const selectionColor = {
      '--md-elevated-button-container-color': this.scan?.color,
    };
    return html`
      <div class="viewport">
        <img src=${mesh} />
        ${this.scan
          ? html` <md-elevated-button
              class="focus-scan"
              style=${styleMap(selectionColor)}
              label="Scan: ${this.scan.id}"
              @click="${this.focusScan}"
            ></md-elevated-button>`
          : undefined}
      </div>
    `;
  }

  static styles = css`
    :host {
      position: relative;

      min-width: 20rem;
      min-height: 20rem;
      display: flex;
    }

    .viewport {
      flex: 1;

      display: flex;
      justify-content: center;
    }

    img {
      object-fit: cover;
    }

    .focus-scan {
      position: absolute;
      top: 0.2rem;
      left: 0.2rem;
    }

    md-elevated-button {
      --md-elevated-button-label-text-color: black;
    }
  `;
}

declare global {
  interface HTMLElementTagNameMap {
    'scan-view': ScanView;
  }
}
