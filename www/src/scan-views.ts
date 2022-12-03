import { LitElement, css, html } from 'lit';
import { customElement } from 'lit/decorators.js';
import { repeat } from 'lit/directives/repeat.js';
import '@material/web/fab/fab.js';

import { compareObjects, connectState } from './utils/select-state.js';
import './feature-scans.js';
import { ContextConsumer } from '@lit-labs/context';
import { FeatureViewId, hasiContext } from './state/hasi.machine.js';
import { NAME_TO_KEY } from './scan.types.js';
import { classMap } from 'lit/directives/class-map.js';

@customElement('scan-views')
export class ScanViews extends LitElement {
  features = connectState(
    this,
    (state) => state.context.features,
    compareObjects
  );

  stateService = new ContextConsumer(this, hasiContext, undefined, true);

  private addHandler() {
    this.stateService.value?.service.send('FEATURE_ADD');
  }

  private valueChangedHandler =
    (featureViewId: FeatureViewId) => (e: CustomEvent) => {
      this.stateService.value?.service.send({
        type: 'FEATURE_SELECT',
        featureViewId,
        feature: NAME_TO_KEY[e.detail.value],
      });
      e.stopPropagation();
    };

  private featureCloseHandler =
    (featureViewId: FeatureViewId) => (e: Event) => {
      this.stateService.value?.service.send({
        type: 'FEATURE_REMOVE',
        featureViewId,
      });
      e.stopPropagation();
    };

  render() {
    return html`
      <div class="feature-grid">
        ${repeat(
          Object.entries(this.features.value ?? {}),
          ([id]) => id,
          ([id, feature]) => html`<feature-scans
            .feature=${feature}
            @feature-close=${this.featureCloseHandler(id)}
            @autocomplete-value-changed=${this.valueChangedHandler(id)}
          ></feature-scans>`
        )}
        <div
          class="add ${classMap({
            fill: Object.keys(this.features.value ?? {}).length === 0,
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
      display: flex;
      overflow: auto;
    }

    .feature-grid > * {
      flex: 1;
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
