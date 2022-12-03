import { LitElement, css, html } from 'lit';
import { customElement, property } from 'lit/decorators.js';
import { styleMap } from 'lit/directives/style-map.js';
import { ContextConsumer } from '@lit-labs/context';
import '@material/web/button/elevated-button.js';

import { Feature } from './scan.types.js';
import { ScanSelection } from './state/scan-selections.js';
import { hasiContext } from './state/hasi.machine.js';

@customElement('scan-view')
export class ScanView extends LitElement {
  @property() scan!: ScanSelection;
  @property() feature!: Feature;

  stateService = new ContextConsumer(this, hasiContext, undefined, true);

  focusScan() {
    this.stateService.value?.service.send({
      type: 'FOCUS_SCAN',
      id: this.scan.id,
    });
  }

  render() {
    const selectionColor = {
      '--md-elevated-button-container-color': this.scan.color,
    };
    const image = `http://placekitten.com/400/300?image=${Math.floor(
      Math.random() * 16
    )}`;
    const background = {
      background: `url(${image}) center center`,
    };
    return html`
      <div style=${styleMap(background)} class="viewport">
        <md-elevated-button
          class="focus-scan"
          style=${styleMap(selectionColor)}
          label="Scan: ${this.scan.id}"
          @click="${this.focusScan}"
        ></md-elevated-button>
      </div>
    `;
  }

  static styles = css`
    :host {
      position: relative;

      min-width: 30rem;
      display: flex;
    }

    .viewport {
      flex: 1;
    }

    .focus-scan {
      position: absolute;
      top: 0.2rem;
      left: 0.2rem;
    }
  `;
}

declare global {
  interface HTMLElementTagNameMap {
    'scan-view': ScanView;
  }
}
