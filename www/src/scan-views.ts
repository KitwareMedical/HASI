import { LitElement, css, html } from 'lit';
import { customElement } from 'lit/decorators.js';
import { map } from 'lit/directives/map.js';
import '@material/web/fab/fab.js';

import { compareArrays, connectState } from './utils/select-state.js';
import './feature-scans.js';
import { ContextConsumer } from '@lit-labs/context';
import { hasiContext } from './state/hasi.machine.js';
import { NAME_TO_KEY } from './scan.types.js';
import { classMap } from 'lit/directives/class-map.js';

@customElement('scan-views')
export class ScanViews extends LitElement {
  features = connectState(
    this,
    (state) => state.context.features,
    compareArrays
  );

  stateService = new ContextConsumer(this, hasiContext, undefined, true);

  private addHandler() {
    this.stateService.value?.service.send('FEATURE_ADD');
  }

  private valueChangedHandler = (featureIndex: number) => (e: CustomEvent) => {
    this.stateService.value?.service.send({
      type: 'FEATURE_SELECT',
      featureIndex,
      feature: NAME_TO_KEY[e.detail.value],
    });
    e.stopPropagation();
  };

  private featureCloseHandler = (featureIndex: number) => (e: Event) => {
    this.stateService.value?.service.send({
      type: 'FEATURE_REMOVE',
      featureIndex,
    });
    e.stopPropagation();
  };

  render() {
    return html`
      <div class="feature-grid">
        ${map(
          this.features.value || [],
          (feature, idx) => html`<feature-scans
            .feature=${feature}
            @feature-close=${this.featureCloseHandler(idx)}
            @autocomplete-value-changed=${this.valueChangedHandler(idx)}
          ></feature-scans>`
        )}
        <div
          class="add ${classMap({
            fill: this.features.value?.length === 0,
          })}"
        >
          <md-standard-icon-button @click="${this.addHandler}" icon="add">
          </md-standard-icon-button>
        </div>
      </div>
    `;
  }

  static styles = css`
    .feature-grid {
      height: 100%;
      flex: 1;

      display: flex;
      overflow: auto;
    }

    .feature-grid > * {
      flex: 1;
      margin-right: 0.2rem;
      min-width: 30rem;
    }

    .add {
      flex: 0 1 auto;
      min-width: auto;

      display: flex;
      align-items: center;
      justify-content: center;
    }

    .fill {
      flex: 1;
    }
  `;
}

declare global {
  interface HTMLElementTagNameMap {
    'scan-views': ScanViews;
  }
}
